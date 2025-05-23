#include <QtWidgets>
#include <QPrinterInfo>
#include <QPrintDialog>
#include <QPageSetupDialog>
#include "dialogprintsettings.h"
#include "dialogautostart.h"
#include "dialogdisplaydefinition.h"
#include "dialogconnectiondetails.h"
#include "dialogmaskwriteregiter.h"
#include "dialogsetuppresetdata.h"
#include "dialogforcemultiplecoils.h"
#include "dialogforcemultipleregisters.h"
#include "dialogusermsg.h"
#include "dialogmsgparser.h"
#include "dialogaddressscan.h"
#include "dialogmodbusscanner.h"
#include "dialogwindowsmanager.h"
#include "dialogabout.h"
#include "mainstatusbar.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ChartDock.h"

///
/// \brief MainWindow::MainWindow
/// \param parent
///
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,_lang("en")
    ,_windowCounter(0)
    ,_autoStart(false)
    ,_selectedPrinter(nullptr)
    ,_dataSimulator(new DataSimulator(this))
{
    ui->setupUi(this);

    setWindowTitle(APP_NAME);
    setUnifiedTitleAndToolBarOnMac(true);
    setStatusBar(new MainStatusBar(_modbusClient, ui->mdiArea));

    _ansiMenu = new AnsiMenu(this);
    connect(_ansiMenu, &AnsiMenu::codepageSelected, this, &MainWindow::setCodepage);
    ui->actionAnsi->setMenu(_ansiMenu);
    qobject_cast<QToolButton*>(ui->toolBarDisplay->widgetForAction(ui->actionAnsi))->setPopupMode(QToolButton::DelayedPopup);

    const auto defaultPrinter = QPrinterInfo::defaultPrinter();
    if(!defaultPrinter.isNull())
        _selectedPrinter = new QPrinter(defaultPrinter);

    _recentFileActionList = new RecentFileActionList(ui->menuFile, ui->actionRecentFile);
    connect(_recentFileActionList, &RecentFileActionList::triggered, this, &MainWindow::openFile);

    _windowActionList = new WindowActionList(ui->menuWindow, ui->actionWindows);
    connect(_windowActionList, &WindowActionList::triggered, this, &MainWindow::windowActivate);

    auto dispatcher = QAbstractEventDispatcher::instance();
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, &MainWindow::on_awake);

    connect(ui->mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::updateMenuWindow);
    connect(&_modbusClient, &ModbusClient::modbusError, this, &MainWindow::on_modbusError);
    connect(&_modbusClient, &ModbusClient::modbusConnectionError, this, &MainWindow::on_modbusConnectionError);
    connect(&_modbusClient, &ModbusClient::modbusConnected, this, &MainWindow::on_modbusConnected);
    connect(&_modbusClient, &ModbusClient::modbusDisconnected, this, &MainWindow::on_modbusDisconnected);

    ChartDock *chartDock = new ChartDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, chartDock);

    chartDock->setMinimumHeight(200);

    connect(&_modbusClient,
            &ModbusClient::registerValueReady,
            chartDock,
            &ChartDock::onRegisterValue);

    ui->actionNew->trigger();
    loadSettings();
}

///
/// \brief MainWindow::~MainWindow
///
MainWindow::~MainWindow()
{
    delete ui;

    if(_selectedPrinter != nullptr)
        delete _selectedPrinter;
}

///
/// \brief MainWindow::setLanguage
/// \param lang
///
void MainWindow::setLanguage(const QString& lang)
{
    if(lang == "en")
    {
        _lang = lang;
        qApp->removeTranslator(&_appTranslator);
        qApp->removeTranslator(&_qtTranslator);
    }
    else if(_appTranslator.load(QString(":/translations/omodscan_%1").arg(lang)))
    {
        _lang = lang;
        qApp->installTranslator(&_appTranslator);

        if(_qtTranslator.load(QString("%1/translations/qt_%2").arg(qApp->applicationDirPath(), lang)))
            qApp->installTranslator(&_qtTranslator);
    }
}

///
/// \brief MainWindow::changeEvent
/// \param event
///
void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }

    QMainWindow::changeEvent(event);
}

///
/// \brief MainWindow::closeEvent
/// \param event
///
void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();

    ui->mdiArea->closeAllSubWindows();
    if (ui->mdiArea->currentSubWindow())
    {
        event->ignore();
    }

    QMainWindow::closeEvent(event);
}

///
/// \brief MainWindow::eventFilter
/// \param obj
/// \param e
/// \return
///
bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    switch (e->type())
    {
        case QEvent::Close:
            _windowActionList->removeWindow(qobject_cast<QMdiSubWindow*>(obj));
        break;
        default:
            qt_noop();
    }
    return QObject::eventFilter(obj, e);
}

///
/// \brief MainWindow::on_awake
///
void MainWindow::on_awake()
{
    auto frm = currentMdiChild();
    const auto state = _modbusClient.state();

    ui->menuSetup->setEnabled(frm != nullptr);
    ui->menuWindow->setEnabled(frm != nullptr);
    ui->menuConfig->setEnabled(frm != nullptr);

    ui->actionSave->setEnabled(frm != nullptr);
    ui->actionSaveAs->setEnabled(frm != nullptr);
    ui->actionPrintSetup->setEnabled(_selectedPrinter != nullptr);
    ui->actionPrint->setEnabled(_selectedPrinter != nullptr && frm && frm->displayMode() == DisplayMode::Data);
    ui->actionRecentFile->setEnabled(!_recentFileActionList->isEmpty());

    ui->actionConnect->setEnabled(state == QModbusDevice::UnconnectedState);
    ui->actionDisconnect->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionQuickConnect->setEnabled(state == QModbusDevice::UnconnectedState);
    ui->actionEnable->setEnabled(!_autoStart);
    ui->actionDisable->setEnabled(_autoStart);
    ui->actionDataDefinition->setEnabled(frm != nullptr);
    ui->actionShowData->setEnabled(frm != nullptr);
    ui->actionShowTraffic->setEnabled(frm != nullptr);
    ui->actionBinary->setEnabled(frm != nullptr);
    ui->actionUInt16->setEnabled(frm != nullptr);
    ui->actionInt16->setEnabled(frm != nullptr);
    ui->actionInt32->setEnabled(frm != nullptr);
    ui->actionSwappedInt32->setEnabled(frm != nullptr);
    ui->actionUInt32->setEnabled(frm != nullptr);
    ui->actionSwappedUInt32->setEnabled(frm != nullptr);
    ui->actionInt64->setEnabled(frm != nullptr);
    ui->actionSwappedInt64->setEnabled(frm != nullptr);
    ui->actionUInt64->setEnabled(frm != nullptr);
    ui->actionSwappedUInt64->setEnabled(frm != nullptr);
    ui->actionHex->setEnabled(frm != nullptr);
    ui->actionAnsi->setEnabled(frm != nullptr);
    ui->actionHex->setEnabled(frm != nullptr);
    ui->actionFloatingPt->setEnabled(frm != nullptr);
    ui->actionSwappedFP->setEnabled(frm != nullptr);
    ui->actionDblFloat->setEnabled(frm != nullptr);
    ui->actionSwappedDbl->setEnabled(frm != nullptr);
    ui->actionSwapBytes->setEnabled(frm != nullptr);

    ui->actionForceCoils->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionPresetRegs->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionMaskWrite->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionUserMsg->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionAddressScan->setEnabled(state == QModbusDevice::ConnectedState);
    ui->actionTextCapture->setEnabled(frm && frm->captureMode() == CaptureMode::Off);
    ui->actionCaptureOff->setEnabled(frm && frm->captureMode() == CaptureMode::TextCapture);
    ui->actionResetCtrs->setEnabled(frm != nullptr);

    ui->actionToolbar->setChecked(ui->toolBarMain->isVisible());
    ui->actionStatusBar->setChecked(statusBar()->isVisible());
    ui->actionDisplayBar->setChecked(ui->toolBarDisplay->isVisible());
    ui->actionEnglish->setChecked(_lang == "en");
    ui->actionRussian->setChecked(_lang == "ru");
    ui->actionChineseCn->setChecked(_lang == "cn");
    ui->actionChineseZh->setChecked(_lang == "zh");

    if(frm != nullptr)
    {
        const auto ddm = frm->dataDisplayMode();
        ui->actionBinary->setChecked(ddm == DataDisplayMode::Binary);
        ui->actionUInt16->setChecked(ddm == DataDisplayMode::UInt16);
        ui->actionInt16->setChecked(ddm == DataDisplayMode::Int16);
        ui->actionInt32->setChecked(ddm == DataDisplayMode::Int32);
        ui->actionSwappedInt32->setChecked(ddm == DataDisplayMode::SwappedInt32);
        ui->actionUInt32->setChecked(ddm == DataDisplayMode::UInt32);
        ui->actionSwappedUInt32->setChecked(ddm == DataDisplayMode::SwappedUInt32);
        ui->actionInt64->setChecked(ddm == DataDisplayMode::Int64);
        ui->actionSwappedInt64->setChecked(ddm == DataDisplayMode::SwappedInt64);
        ui->actionUInt64->setChecked(ddm == DataDisplayMode::UInt64);
        ui->actionSwappedUInt64->setChecked(ddm == DataDisplayMode::SwappedUInt64);
        ui->actionHex->setChecked(ddm == DataDisplayMode::Hex);
        ui->actionAnsi->setChecked(ddm == DataDisplayMode::Ansi);
        ui->actionFloatingPt->setChecked(ddm == DataDisplayMode::FloatingPt);
        ui->actionSwappedFP->setChecked(ddm == DataDisplayMode::SwappedFP);
        ui->actionDblFloat->setChecked(ddm == DataDisplayMode::DblFloat);
        ui->actionSwappedDbl->setChecked(ddm == DataDisplayMode::SwappedDbl);

        const auto byteOrder = frm->byteOrder();
        ui->actionSwapBytes->setChecked(byteOrder == ByteOrder::Swapped);

        ui->actionHexAddresses->setChecked(frm->displayHexAddresses());

        const auto dm = frm->displayMode();
        ui->actionShowData->setChecked(dm == DisplayMode::Data);
        ui->actionShowTraffic->setChecked(dm == DisplayMode::Traffic);
    }
}

///
/// \brief MainWindow::on_modbusError
/// \param error
/// \param requestId
///
void MainWindow::on_modbusError(const QString& error, int requestId)
{
    if(0 == requestId)
        QMessageBox::warning(this, windowTitle(), error);
}

///
/// \brief MainWindow::on_modbusConnectionError
/// \param error
///
void MainWindow::on_modbusConnectionError(const QString& error)
{
    _modbusClient.disconnectDevice();
    QMessageBox::warning(this, windowTitle(), error);
}

///
/// \brief MainWindow::on_modbusConnected
///
void MainWindow::on_modbusConnected(const ConnectionDetails&)
{
    _dataSimulator->resumeSimulations();
}

///
/// \brief MainWindow::on_modbusDisconnected
///
void MainWindow::on_modbusDisconnected(const ConnectionDetails&)
{
    _dataSimulator->pauseSimulations();
}

///
/// \brief MainWindow::on_actionNew_triggered
///
void MainWindow::on_actionNew_triggered()
{
    const auto cur = currentMdiChild();
    auto frm = createMdiChild(++_windowCounter);

    if(cur) {
        frm->setByteOrder(cur->byteOrder());
        frm->setCodepage(cur->codepage());
        frm->setDisplayMode(cur->displayMode());
        frm->setDataDisplayMode(cur->dataDisplayMode());
        frm->setDisplayDefinition(cur->displayDefinition());

        frm->setFont(cur->font());
        frm->setStatusColor(cur->statusColor());
        frm->setBackgroundColor(cur->backgroundColor());
        frm->setForegroundColor(cur->foregroundColor());
    }

    frm->show();
}

///
/// \brief MainWindow::on_actionOpen_triggered
///
void MainWindow::on_actionOpen_triggered()
{
    const auto filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("All files (*)"));
    if(filename.isEmpty()) return;

    openFile(filename);
}

///
/// \brief MainWindow::on_actionClose_triggered
///
void MainWindow::on_actionClose_triggered()
{
    const auto wnd = ui->mdiArea->currentSubWindow();
    if(!wnd) return;

    wnd->close();
}

///
/// \brief MainWindow::on_actionSave_triggered
///
void MainWindow::on_actionSave_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    if(frm->filename().isEmpty())
        ui->actionSaveAs->trigger();
    else
        saveMdiChild(frm);
}

///
/// \brief MainWindow::on_actionSaveAs_triggered
///
void MainWindow::on_actionSaveAs_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    const auto filename = QFileDialog::getSaveFileName(this, QString(), frm->windowTitle(), tr("All files (*)"));
    if(filename.isEmpty()) return;

    frm->setFilename(filename);

    saveMdiChild(frm);
}

///
/// \brief MainWindow::on_actionPrint_triggered
///
void MainWindow::on_actionPrint_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    QPrintDialog dlg(_selectedPrinter, this);
    if(dlg.exec() == QDialog::Accepted)
    {
        frm->print(_selectedPrinter);
    }
}

///
/// \brief MainWindow::on_actionPrintSetup_triggered
///
void MainWindow::on_actionPrintSetup_triggered()
{
    DialogPrintSettings dlg(_selectedPrinter, this);
    dlg.exec();
}

///
/// \brief MainWindow::on_actionExit_triggered
///
void MainWindow::on_actionExit_triggered()
{
    close();
}

///
/// \brief MainWindow::on_actionConnect_triggered
///
void MainWindow::on_actionConnect_triggered()
{
    DialogConnectionDetails dlg(_connParams, this);
    if(dlg.exec() == QDialog::Accepted)
    {
        ui->actionQuickConnect->trigger();
    }
}

///
/// \brief MainWindow::on_actionDisconnect_triggered
///
void MainWindow::on_actionDisconnect_triggered()
{
    _modbusClient.disconnectDevice();
}

///
/// \brief MainWindow::on_actionQuickConnect_triggered
///
void MainWindow::on_actionQuickConnect_triggered()
{
    _modbusClient.connectDevice(_connParams);
}

///
/// \brief MainWindow::on_actionEnable_triggered
///
void MainWindow::on_actionEnable_triggered()
{
    DialogAutoStart dlg(_fileAutoStart, this);
    _autoStart = dlg.exec() == QDialog::Accepted;
}

///
/// \brief MainWindow::on_actionDisable_triggered
///
void MainWindow::on_actionDisable_triggered()
{
    _autoStart = false;
}

///
/// \brief MainWindow::on_actionSaveConfig_triggered
///
void MainWindow::on_actionSaveConfig_triggered()
{
    const auto filename = QFileDialog::getSaveFileName(this, QString(), QString(), tr("All files (*)"));
    if(filename.isEmpty()) return;

    saveConfig(filename);
}

///
/// \brief MainWindow::on_actionRestoreNow_triggered
///
void MainWindow::on_actionRestoreNow_triggered()
{
    const auto filename = QFileDialog::getOpenFileName(this, QString(), QString(), tr("All files (*)"));
    if(filename.isEmpty()) return;

    loadConfig(filename);
}

///
/// \brief MainWindow::on_actionModbusScanner_triggered
///
void MainWindow::on_actionModbusScanner_triggered()
{
    auto dlg = new DialogModbusScanner(this);
    connect(dlg, &DialogModbusScanner::attemptToConnect, this,
    [this](const ConnectionDetails& cd, int deviceId)
    {
        _connParams = cd;
        _modbusClient.disconnectDevice();
        _modbusClient.connectDevice(_connParams);

        auto frm = currentMdiChild();
        if(frm)
        {
            auto dd = frm->displayDefinition();
            dd.DeviceId = deviceId;
            frm->setDisplayDefinition(dd);
        }
    });

    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
}

///
/// \brief MainWindow::on_actionDataDefinition_triggered
///
void MainWindow::on_actionDataDefinition_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    DialogDisplayDefinition dlg(frm->displayDefinition(), this);
    if(dlg.exec() == QDialog::Accepted)
        frm->setDisplayDefinition(dlg.displayDefinition());
}

///
/// \brief MainWindow::on_actionShowData_triggered
///
void MainWindow::on_actionShowData_triggered()
{
    auto frm = currentMdiChild();
    if(frm) frm->setDisplayMode(DisplayMode::Data);
}

///
/// \brief MainWindow::on_actionShowTraffic_triggered
///
void MainWindow::on_actionShowTraffic_triggered()
{
    auto frm = currentMdiChild();
    if(frm) frm->setDisplayMode(DisplayMode::Traffic);
}

///
/// \brief MainWindow::on_actionBinary_triggered
///
void MainWindow::on_actionBinary_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Binary);
}

///
/// \brief MainWindow::on_actionUInt16_triggered
///
void MainWindow::on_actionUInt16_triggered()
{
    updateDataDisplayMode(DataDisplayMode::UInt16);
}

///
/// \brief MainWindow::on_actionInt16_triggered
///
void MainWindow::on_actionInt16_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Int16);
}

///
/// \brief MainWindow::on_actionInt32_triggered
///
void MainWindow::on_actionInt32_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Int32);
}

///
/// \brief MainWindow::on_actionSwappedInt32_triggered
///
void MainWindow::on_actionSwappedInt32_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedInt32);
}

///
/// \brief MainWindow::on_actionUInt32_triggered
///
void MainWindow::on_actionUInt32_triggered()
{
    updateDataDisplayMode(DataDisplayMode::UInt32);
}

///
/// \brief MainWindow::on_actionSwappedUInt32_triggered
///
void MainWindow::on_actionSwappedUInt32_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedUInt32);
}

///
/// \brief MainWindow::on_actionInt64_triggered
///
void MainWindow::on_actionInt64_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Int64);
}

///
/// \brief MainWindow::on_actionSwappedInt64_triggered
///
void MainWindow::on_actionSwappedInt64_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedInt64);
}

///
/// \brief MainWindow::on_actionUInt64_triggered
///
void MainWindow::on_actionUInt64_triggered()
{
    updateDataDisplayMode(DataDisplayMode::UInt64);
}

///
/// \brief MainWindow::on_actionSwappedUInt64_triggered
///
void MainWindow::on_actionSwappedUInt64_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedUInt64);
}

///
/// \brief MainWindow::on_actionHex_triggered
///
void MainWindow::on_actionHex_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Hex);
}

///
/// \brief MainWindow::on_actionAnsi_triggered
///
void MainWindow::on_actionAnsi_triggered()
{
    updateDataDisplayMode(DataDisplayMode::Ansi);
}

///
/// \brief MainWindow::on_actionFloatingPt_triggered
///
void MainWindow::on_actionFloatingPt_triggered()
{
    updateDataDisplayMode(DataDisplayMode::FloatingPt);
}

///
/// \brief MainWindow::on_actionSwappedFP_triggered
///
void MainWindow::on_actionSwappedFP_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedFP);
}

///
/// \brief MainWindow::on_actionDblFloat_triggered
///
void MainWindow::on_actionDblFloat_triggered()
{
    updateDataDisplayMode(DataDisplayMode::DblFloat);
}

///
/// \brief MainWindow::on_actionSwappedDbl_triggered
///
void MainWindow::on_actionSwappedDbl_triggered()
{
    updateDataDisplayMode(DataDisplayMode::SwappedDbl);
}

///
/// \brief MainWindow::on_actionSwapBytes_triggered
///
void MainWindow::on_actionSwapBytes_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    switch (frm->byteOrder()) {
        case ByteOrder::Swapped:
            frm->setByteOrder(ByteOrder::Direct);
        break;
        case ByteOrder::Direct:
            frm->setByteOrder(ByteOrder::Swapped);
        break;
    }
}

///
/// \brief MainWindow::on_actionHexAddresses_triggered
///
void MainWindow::on_actionHexAddresses_triggered()
{
    auto frm = currentMdiChild();
    if(frm) frm->setDisplayHexAddresses(!frm->displayHexAddresses());
}

///
/// \brief MainWindow::on_actionForceCoils_triggered
///
void MainWindow::on_actionForceCoils_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    const auto dd = frm->displayDefinition();
    SetupPresetParams presetParams = { dd.DeviceId, dd.PointAddress, dd.Length, dd.ZeroBasedAddress };

    {
        DialogSetupPresetData dlg(presetParams, QModbusDataUnit::Coils, this);
        if(dlg.exec() != QDialog::Accepted) return;
    }

    ModbusWriteParams params;
    params.Node = presetParams.SlaveAddress;
    params.Address = presetParams.PointAddress;
    params.ZeroBasedAddress = dd.ZeroBasedAddress;

    if(dd.PointType == QModbusDataUnit::Coils &&
       dd.DeviceId == params.Node &&
       dd.PointAddress == params.Address)
    {
        params.Value = QVariant::fromValue(frm->data());
    }

    DialogForceMultipleCoils dlg(params, presetParams.Length, this);
    if(dlg.exec() == QDialog::Accepted)
    {
        _modbusClient.writeRegister(QModbusDataUnit::Coils, params, 0);
    }
}

///
/// \brief MainWindow::on_actionPresetRegs_triggered
///
void MainWindow::on_actionPresetRegs_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    const auto dd = frm->displayDefinition();
    SetupPresetParams presetParams = { dd.DeviceId, dd.PointAddress, dd.Length, dd.ZeroBasedAddress };

    {
        DialogSetupPresetData dlg(presetParams, QModbusDataUnit::HoldingRegisters, this);
        if(dlg.exec() != QDialog::Accepted) return;
    }

    ModbusWriteParams params;
    params.Node = presetParams.SlaveAddress;
    params.Address = presetParams.PointAddress;
    params.DisplayMode = frm->dataDisplayMode();
    params.Order = frm->byteOrder();
    params.Codepage = frm->codepage();
    params.ZeroBasedAddress = dd.ZeroBasedAddress;

    if(dd.PointType == QModbusDataUnit::HoldingRegisters &&
       dd.DeviceId == params.Node &&
       dd.PointAddress == params.Address)
    {
        params.Value = QVariant::fromValue(frm->data());
    }

    DialogForceMultipleRegisters dlg(params, presetParams.Length, this);
    if(dlg.exec() == QDialog::Accepted)
    {
        _modbusClient.writeRegister(QModbusDataUnit::HoldingRegisters, params, 0);
    }
}

///
/// \brief MainWindow::on_actionMaskWrite_triggered
///
void MainWindow::on_actionMaskWrite_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    const auto dd = frm->displayDefinition();
    ModbusMaskWriteParams params = { dd.DeviceId, dd.PointAddress, 0xFFFF, 0, dd.ZeroBasedAddress};

    DialogMaskWriteRegiter dlg(params, this);
    if(dlg.exec() == QDialog::Accepted)
    {
        _modbusClient.maskWriteRegister(params, 0);
    }
}

///
/// \brief MainWindow::on_actionUserMsg_triggered
///
void MainWindow::on_actionUserMsg_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    const auto dd = frm->displayDefinition();
    const auto mode = frm->dataDisplayMode();

    QModbusPdu::FunctionCode func;
    switch(dd.PointType)
    {
        case QModbusDataUnit::Coils:
            func = QModbusPdu::ReadCoils;
        break;

        case QModbusDataUnit::DiscreteInputs:
            func = QModbusPdu::ReadDiscreteInputs;
        break;

        case QModbusDataUnit::HoldingRegisters:
            func = QModbusPdu::ReadHoldingRegisters;
        break;

        case QModbusDataUnit::InputRegisters:
            func = QModbusPdu::ReadInputRegisters;
        break;

        default:
            func = QModbusPdu::Invalid;
        break;
    }

    DialogUserMsg dlg(dd.DeviceId, func, mode, _modbusClient, this);
    dlg.exec();
}

///
/// \brief MainWindow::on_actionMsgParser_triggered
///
void MainWindow::on_actionMsgParser_triggered()
{
    auto frm = currentMdiChild();
    const auto mode = frm ? frm->dataDisplayMode() : DataDisplayMode::Hex;
    const auto protocol = _modbusClient.connectionType() == ConnectionType::Serial ? ModbusMessage::Rtu : ModbusMessage::Tcp;

    auto dlg = new DialogMsgParser(mode, protocol, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
}

///
/// \brief MainWindow::on_actionAddressScan_triggered
///
void MainWindow::on_actionAddressScan_triggered()
{
    auto frm = currentMdiChild();
    const auto dd = frm ? frm->displayDefinition() : DisplayDefinition();
    const auto mode = frm ? frm->dataDisplayMode() : DataDisplayMode::UInt16;
    const auto order = frm ? frm->byteOrder() : ByteOrder::Direct;

    auto dlg = new DialogAddressScan(dd, mode, order, _modbusClient, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
}

///
/// \brief MainWindow::on_actionTextCapture_triggered
///
void MainWindow::on_actionTextCapture_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    auto filename = QFileDialog::getSaveFileName(this, QString(), QString(), "Text files (*.txt)");
    if(!filename.isEmpty())
    {
        if(!filename.endsWith(".txt", Qt::CaseInsensitive)) filename += ".txt";
        frm->startTextCapture(filename);
    }
}

///
/// \brief MainWindow::on_actionCaptureOff_triggered
///
void MainWindow::on_actionCaptureOff_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    frm->stopTextCapture();
}

///
/// \brief MainWindow::on_actionResetCtrs_triggered
///
void MainWindow::on_actionResetCtrs_triggered()
{
    auto frm = currentMdiChild();
    if(frm) frm->resetCtrs();
}

///
/// \brief MainWindow::setCodepage
/// \param name
///
void MainWindow::setCodepage(const QString& name)
{
    auto frm = currentMdiChild();
    if(!frm) return;

    frm->setCodepage(name);
}

///
/// \brief MainWindow::on_actionToolbar_triggered
///
void MainWindow::on_actionToolbar_triggered()
{
    ui->toolBarMain->setVisible(!ui->toolBarMain->isVisible());
}

///
/// \brief MainWindow::on_actionStatusBar_triggered
///
void MainWindow::on_actionStatusBar_triggered()
{
    ui->statusbar->setVisible(!ui->statusbar->isVisible());
}

///
/// \brief MainWindow::on_actionDisplayBar_triggered
///
void MainWindow::on_actionDisplayBar_triggered()
{
    ui->toolBarDisplay->setVisible(!ui->toolBarDisplay->isVisible());
}

///
/// \brief MainWindow::on_actionBackground_triggered
///
void MainWindow::on_actionBackground_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    QColorDialog dlg(frm->backgroundColor(), this);
    if(dlg.exec() == QDialog::Accepted)
    {
        frm->setBackgroundColor(dlg.currentColor());
    }
}

///
/// \brief MainWindow::on_actionForeground_triggered
///
void MainWindow::on_actionForeground_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    QColorDialog dlg(frm->foregroundColor(), this);
    if(dlg.exec() == QDialog::Accepted)
    {
        frm->setForegroundColor(dlg.currentColor());
    }
}

///
/// \brief MainWindow::on_actionStatus_triggered
///
void MainWindow::on_actionStatus_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    QColorDialog dlg(frm->statusColor(), this);
    if(dlg.exec() == QDialog::Accepted)
    {
        frm->setStatusColor(dlg.currentColor());
    }
}

///
/// \brief MainWindow::on_actionFont_triggered
///
void MainWindow::on_actionFont_triggered()
{
    auto frm = currentMdiChild();
    if(!frm) return;

    QFontDialog dlg(frm->font(), this);
    if(dlg.exec() == QDialog::Accepted)
    {
        frm->setFont(dlg.currentFont());
    }
}

///
/// \brief MainWindow::on_actionEnglish_triggered
///
void MainWindow::on_actionEnglish_triggered()
{
    setLanguage("en");
}

///
/// \brief MainWindow::on_actionRussian_triggered
///
void MainWindow::on_actionRussian_triggered()
{
   setLanguage("ru");
}

///
/// \brief MainWindow::on_actionChineseCn_triggered
///
void MainWindow::on_actionChineseCn_triggered()
{
    setLanguage("cn");
}

///
/// \brief MainWindow::on_actionChineseZh_triggered
///
void MainWindow::on_actionChineseZh_triggered()
{
    setLanguage("zh");
}

///
/// \brief MainWindow::on_actionCascade_triggered
///
void MainWindow::on_actionCascade_triggered()
{
    ui->mdiArea->cascadeSubWindows();
}

///
/// \brief MainWindow::on_actionTile_triggered
///
void MainWindow::on_actionTile_triggered()
{
    ui->mdiArea->tileSubWindows();
}

///
/// \brief MainWindow::on_actionWindows_triggered
///
void MainWindow::on_actionWindows_triggered()
{
    DialogWindowsManager dlg(_windowActionList->actionList(), ui->actionSave, this);
    dlg.exec();
}

///
/// \brief MainWindow::on_actionAbout_triggered
///
void MainWindow::on_actionAbout_triggered()
{
    DialogAbout dlg(this);
    dlg.exec();
}

///
/// \brief MainWindow::updateMenuWindow
///
void MainWindow::updateMenuWindow()
{
    const auto activeWnd = ui->mdiArea->activeSubWindow();
    for(auto&& wnd : ui->mdiArea->subWindowList())
    {
        wnd->setProperty("isActive", wnd == activeWnd);
        wnd->widget()->setProperty("isActive", wnd == activeWnd);
    }
    _windowActionList->update();
}

///
/// \brief MainWindow::windowActivate
/// \param wnd
///
void MainWindow::windowActivate(QMdiSubWindow* wnd)
{
    if(wnd)
    {
        ui->mdiArea->setActiveSubWindow(wnd);
    }
}

///
/// \brief MainWindow::openFile
/// \param filename
///
void MainWindow::openFile(const QString& filename)
{
    auto frm = loadMdiChild(filename);
    if(frm)
    {
        frm->show();
    }
    else
    {
        QString message = !QFileInfo::exists(filename) ?
                    QString(tr("%1 was not found")).arg(filename) :
                    QString(tr("Failed to open %1")).arg(filename);

        _recentFileActionList->removeRecentFile(filename);
        QMessageBox::warning(this, windowTitle(), message);
    }
}

///
/// \brief MainWindow::addRecentFile
/// \param filename
///
void MainWindow::addRecentFile(const QString& filename)
{
    _recentFileActionList->addRecentFile(filename);
}

///
/// \brief MainWindow::updateDisplayMode
/// \param mode
///
void MainWindow::updateDataDisplayMode(DataDisplayMode mode)
{
    auto frm = currentMdiChild();
    if(frm) frm->setDataDisplayMode(mode);
}

///
/// \brief MainWindow::createMdiChild
/// \param id
/// \return
///
FormModSca* MainWindow::createMdiChild(int id)
{
    auto frm = new FormModSca(id, _modbusClient, _dataSimulator, this);
    auto wnd = ui->mdiArea->addSubWindow(frm);
    wnd->installEventFilter(this);
    wnd->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(frm, &FormModSca::showed, this, [this, wnd]
    {
        windowActivate(wnd);
    });

    auto updateCodepage = [this](const QString& name)
    {
        _ansiMenu->selectCodepage(name);
    };

    connect(wnd, &QMdiSubWindow::windowStateChanged, this, [frm, updateCodepage](Qt::WindowStates, Qt::WindowStates newState)
    {
        if(newState == Qt::WindowActive) {
            updateCodepage(frm->codepage());
        }
    });

    connect(frm, &FormModSca::codepageChanged, this, [updateCodepage](const QString& name)
    {
        updateCodepage(name);
    });

    connect(frm, &FormModSca::numberOfPollsChanged, this, [this](uint)
    {
        qobject_cast<MainStatusBar*>(statusBar())->updateNumberOfPolls();
    });

    connect(frm, &FormModSca::validSlaveResposesChanged, this, [this](uint)
    {
        qobject_cast<MainStatusBar*>(statusBar())->updateValidSlaveResponses();
    });

    _windowActionList->addWindow(wnd);

    return frm;
}

///
/// \brief MainWindow::currentMdiChild
/// \return
///
FormModSca* MainWindow::currentMdiChild() const
{
    const auto wnd = ui->mdiArea->currentSubWindow();
    return wnd ? qobject_cast<FormModSca*>(wnd->widget()) : nullptr;
}

///
/// \brief MainWindow::findMdiChild
/// \param num
/// \return
///
FormModSca* MainWindow::findMdiChild(int id) const
{
    for(auto&& wnd : ui->mdiArea->subWindowList())
    {
        const auto frm = qobject_cast<FormModSca*>(wnd->widget());
        if(frm && frm->formId() == id) return frm;
    }
    return nullptr;
}

///
/// \brief MainWindow::firstMdiChild
/// \return
///
FormModSca* MainWindow::firstMdiChild() const
{
    for(auto&& wnd : ui->mdiArea->subWindowList())
        return qobject_cast<FormModSca*>(wnd->widget());

    return nullptr;
}

///
/// \brief MainWindow::loadMdiChild
/// \param filename
/// \return
///
FormModSca* MainWindow::loadMdiChild(const QString& filename)
{
    QFile file(filename);
    if(!file.open(QFile::ReadOnly))
        return nullptr;

    QDataStream s(&file);
    s.setByteOrder(QDataStream::BigEndian);
    s.setVersion(QDataStream::Version::Qt_5_0);

    quint8 magic = 0;
    s >> magic;

    if(magic != 0x32)
        return nullptr;

    QVersionNumber ver;
    s >> ver;

    if(ver > FormModSca::VERSION)
        return nullptr;

    int formId;
    s >> formId;

    if(s.status() != QDataStream::Ok)
        return nullptr;

    bool created = false;
    auto frm = findMdiChild(formId);
    if(!frm)
    {
        created = true;
        frm = createMdiChild(formId);
    }

    if(!frm)
        return nullptr;

    frm->setProperty("Version", QVariant::fromValue(ver));
    s >> frm;

    if(s.status() != QDataStream::Ok)
    {
        if(created) frm->close();
        return nullptr;
    }

    frm->setFilename(filename);
    addRecentFile(filename);
    _windowCounter = qMax(frm->formId(), _windowCounter);

    return frm;
}

///
/// \brief MainWindow::saveMdiChild
/// \param frm
///
void MainWindow::saveMdiChild(FormModSca* frm)
{
    if(!frm) return;

    QFile file(frm->filename());
    if(!file.open(QFile::WriteOnly))
        return;

    QDataStream s(&file);
    s.setByteOrder(QDataStream::BigEndian);
    s.setVersion(QDataStream::Version::Qt_5_0);

    // magic number
    s << (quint8)0x32;

    // version number
    s << FormModSca::VERSION;

    // form
    s << frm;

    addRecentFile(frm->filename());
}

///
/// \brief MainWindow::loadConfig
/// \param filename
///
void MainWindow::loadConfig(const QString& filename)
{
    QFile file(filename);
    if(!file.open(QFile::ReadOnly))
        return;

    QDataStream s(&file);
    s.setByteOrder(QDataStream::BigEndian);
    s.setVersion(QDataStream::Version::Qt_5_0);

    quint8 magic = 0;
    s >> magic;

    if(magic != 0x33)
        return;

    QVersionNumber ver;
    s >> ver;

    if(ver != QVersionNumber(1, 0))
        return;

    QStringList listFilename;
    s >> listFilename;

    ConnectionDetails connParams;
    s >> connParams;

    bool connected;
    s >> connected;

    if(s.status() != QDataStream::Ok)
        return;

    ui->mdiArea->closeAllSubWindows();
    for(auto&& filename: listFilename)
    {
        if(!filename.isEmpty())
            openFile(filename);
    }

    _connParams = connParams;
    if(connected) ui->actionQuickConnect->trigger();
}

///
/// \brief MainWindow::saveConfig
/// \param filename
///
void MainWindow::saveConfig(const QString& filename)
{
    QFile file(filename);
    if(!file.open(QFile::WriteOnly))
        return;

    QStringList listFilename;
    const auto activeWnd = ui->mdiArea->currentSubWindow();
    for(auto&& wnd : ui->mdiArea->subWindowList())
    {
        windowActivate(wnd);
        ui->actionSave->trigger();

        const auto frm = qobject_cast<FormModSca*>(wnd->widget());
        const auto filename = frm->filename();
        if(!filename.isEmpty()) listFilename.push_back(filename);
    }
    windowActivate(activeWnd);

    QDataStream s(&file);
    s.setByteOrder(QDataStream::BigEndian);
    s.setVersion(QDataStream::Version::Qt_5_0);

    // magic number
    s << (quint8)0x33;

    // version number
    s << QVersionNumber(1, 0);

    // list of files
    s << listFilename;

    // connection params
    s << _connParams;

    // connection state
    s << (_modbusClient.state() == QModbusDevice::ConnectedState);
}

///
/// \brief MainWindow::loadSettings
///
void MainWindow::loadSettings()
{
    const auto filename = QString("%1.ini").arg(QFileInfo(qApp->applicationFilePath()).baseName());
    auto filepath = QString("%1%2%3").arg(qApp->applicationDirPath(), QDir::separator(), filename);

    if(!QFile::exists(filepath))
        filepath = QString("%1%2%3").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
                                         QDir::separator(), filename);

    if(!QFile::exists(filepath)) return;

    QSettings m(filepath, QSettings::IniFormat, this);

    const auto toolbarArea = (Qt::ToolBarArea)qBound(0, m.value("DisplayBarArea").toInt(), 0xf);
    const auto toolbarBreal = m.value("DisplayBarBreak").toBool();
    if(toolbarBreal) addToolBarBreak(toolbarArea);
    addToolBar(toolbarArea, ui->toolBarDisplay);

    _autoStart = m.value("AutoStart").toBool();
    _fileAutoStart = m.value("StartUpFile").toString();

    _lang = m.value("Language", "en").toString();
    setLanguage(_lang);

    m >> firstMdiChild();
    m >> _connParams;

    if(_autoStart)
    {
        loadConfig(_fileAutoStart);
    }
}

///
/// \brief checkPathIsWritable
/// \param path
/// \return
///
bool checkPathIsWritable(const QString& path)
{
    const auto filepath = QString("%1%2%3").arg(path, QDir::separator(), ".test");
    if(!QFile(filepath).open(QFile::WriteOnly)) return false;

    QFile::remove(filepath);
    return true;
}

///
/// \brief MainWindow::saveSettings
///
void MainWindow::saveSettings()
{
    const auto filename = QString("%1.ini").arg(QFileInfo(qApp->applicationFilePath()).baseName());
    auto filepath = QString("%1%2%3").arg(qApp->applicationDirPath(), QDir::separator(), filename);

    if(!QFileInfo(qApp->applicationDirPath()).isWritable() || !checkPathIsWritable(qApp->applicationDirPath()))
        filepath = QString("%1%2%3").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
                                         QDir::separator(), filename);

    QSettings m(filepath, QSettings::IniFormat, this);

    m.setValue("DisplayBarArea", toolBarArea(ui->toolBarDisplay));
    m.setValue("DisplayBarBreak", toolBarBreak(ui->toolBarDisplay));

    m.setValue("AutoStart", _autoStart);
    m.setValue("StartUpFile", _fileAutoStart);
    m.setValue("Language", _lang);

    m << firstMdiChild();
    m << _connParams;
}
