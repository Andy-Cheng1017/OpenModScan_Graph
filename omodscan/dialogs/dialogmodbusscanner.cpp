#include <QDateTime>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QAbstractEventDispatcher>
#include "modbuslimits.h"
#include "serialportutils.h"
#include "modbusrtuscanner.h"
#include "modbustcpscanner.h"
#include "dialogmodbusscanner.h"
#include "ui_dialogmodbusscanner.h"

///
/// \brief Parity_toString
/// \param parity
/// \return
///
inline QString Parity_toString(QSerialPort::Parity parity)
{
    switch(parity)
    {
        case QSerialPort::NoParity:
        return DialogModbusScanner::tr("None");

        case QSerialPort::EvenParity:
        return DialogModbusScanner::tr("Even");

        case QSerialPort::OddParity:
        return DialogModbusScanner::tr("Odd");

        case QSerialPort::SpaceParity:
        return DialogModbusScanner::tr("Space");

        case QSerialPort::MarkParity:
        return DialogModbusScanner::tr("Mark");

        default:
        break;
    }

    return QString();
}

///
/// \brief DialogRtuScanner::DialogRtuScanner
/// \param parent
///
DialogModbusScanner::DialogModbusScanner(QWidget *parent)
    : QFixedSizeDialog(parent)
    , ui(new Ui::DialogModbusScanner)
    ,_rtuFuncCode(QModbusPdu::ReportServerId)
    ,_tcpFuncCode(QModbusPdu::ReadHoldingRegisters)
    ,_iconStart(":/res/iconScanStart.png")
    ,_iconStop(":/res/iconScanStop.png")
{
    ui->setupUi(this);
    ui->progressBar->setAlignment(Qt::AlignCenter);
    ui->pushButtonScan->setIcon(_iconStart);

    ui->radioButtonTCP->click();
    ui->comboBoxSerial->addItems(getAvailableSerialPorts());

    QHostAddress address, mask;
    for(auto&& eth : QNetworkInterface::allInterfaces()) {
        if((eth.flags() & QNetworkInterface::IsRunning) && !(eth.flags() & QNetworkInterface::IsLoopBack)){
            for (auto&& entry : eth.addressEntries()) {
                if(entry.ip().protocol() == QAbstractSocket::IPv4Protocol){
                    address = entry.ip();
                    mask = entry.netmask();
                    break;
                }
            }
        }
    }

    ui->lineEditIPAddressFrom->setValue(address);
    ui->lineEditIPAddressTo->setValue(address);
    ui->lineEditSubnetMask->setValue(mask);

    ui->comboBoxFunction->addItem(QModbusPdu::ReadCoils);
    ui->comboBoxFunction->addItem(QModbusPdu::ReadDiscreteInputs);
    ui->comboBoxFunction->addItem(QModbusPdu::ReadHoldingRegisters);
    ui->comboBoxFunction->addItem(QModbusPdu::ReadInputRegisters);
    ui->comboBoxFunction->addItem(QModbusPdu::ReportServerId);
    ui->comboBoxFunction->setCurrentFunctionCode(QModbusPdu::ReadHoldingRegisters);
    ui->comboBoxAddressBase->setCurrentAddressBase(AddressBase::Base1);

    auto dispatcher = QAbstractEventDispatcher::instance();
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, &DialogModbusScanner::on_awake);
}

///
/// \brief DialogRtuScanner::~DialogRtuScanner
///
DialogModbusScanner::~DialogModbusScanner()
{
    delete ui;
}

///
/// \brief DialogRtuScanner::changeEvent
/// \param event
///
void DialogModbusScanner::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);

    QDialog::changeEvent(event);
}

///
/// \brief DialogModbusScanner::showEvent
/// \param e
///
void DialogModbusScanner::showEvent(QShowEvent* e)
{
    QFixedSizeDialog::showEvent(e);
    ui->radioButtonRTU->click();
}

///
/// \brief DialogRtuScanner::on_awake
///
void DialogModbusScanner::on_awake()
{
    const bool inProgress = _scanner && _scanner->inProgress();
    const bool rtuScanning = ui->radioButtonRTU->isChecked();
    ui->comboBoxSerial->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxConnection->setEnabled(!inProgress);
    ui->groupBoxBaudRate->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxDataBits->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxParity->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxStopBits->setEnabled(!inProgress && rtuScanning);
    ui->groupBoxDeviceId->setEnabled(!inProgress);
    ui->groupBoxTimeout->setEnabled(!inProgress);
    ui->groupBoxIPAddressRange->setEnabled(!inProgress);
    ui->groupBoxPortRange->setEnabled(!inProgress);
    ui->groupBoxSubnetMask->setEnabled(!inProgress);
    ui->groupBoxRequest->setEnabled(!inProgress);
    ui->pushButtonClear->setEnabled(!inProgress);
    ui->pushButtonScan->setEnabled((rtuScanning && ui->comboBoxSerial->count() > 0) || !rtuScanning);
    ui->pushButtonScan->setText(inProgress ? tr("Stop") : tr("Start"));
}

///
/// \brief DialogRtuScanner::on_pushButtonScan_clicked
///
void DialogModbusScanner::on_pushButtonScan_clicked()
{
    setCursor(Qt::WaitCursor);

    if(_scanner && _scanner->inProgress())
    {
        stopScan();
    }
    else
    {
        startScan();
    }

    setCursor(Qt::ArrowCursor);
}

///
/// \brief DialogRtuScanner::on_pushButtonClear_clicked
///
void DialogModbusScanner::on_pushButtonClear_clicked()
{
    ui->listWidget->clear();
}

///
/// \brief DialogRtuScanner::on_errorOccurred
/// \param error
///
void DialogModbusScanner::on_errorOccurred(const QString& error)
{
    QMessageBox::warning(this, windowTitle(), error);
}

///
/// \brief DialogModbusScanner::on_comboBoxFunction_functionCodeChanged
/// \param funcCode
///
void DialogModbusScanner::on_comboBoxFunction_functionCodeChanged(QModbusPdu::FunctionCode funcCode)
{
    if(ui->radioButtonRTU->isChecked())
        _rtuFuncCode = funcCode;
    else
        _tcpFuncCode = funcCode;

    ui->spinBoxAddress->setEnabled(funcCode != QModbusPdu::ReportServerId);
    ui->spinBoxLength->setEnabled(funcCode != QModbusPdu::ReportServerId);
    ui->comboBoxAddressBase->setEnabled(funcCode != QModbusPdu::ReportServerId);
}

///
/// \brief DialogModbusScanner::on_comboBoxAddressBase_addressBaseChanged
/// \param base
///
void DialogModbusScanner::on_comboBoxAddressBase_addressBaseChanged(AddressBase base)
{
    const auto addr = ui->spinBoxAddress->value();
    ui->spinBoxAddress->setMinimum(ModbusLimits::addressRange(base == AddressBase::Base0).from());
    ui->spinBoxAddress->setValue(base == AddressBase::Base1 ? qMax(1, addr + 1) : qMax(0, addr - 1));
}

///
/// \brief DialogRtuScanner::on_listWidget_itemDoubleClicked
/// \param item
///
void DialogModbusScanner::on_listWidget_itemDoubleClicked(QListWidgetItem *item)
{
    stopScan();

    const auto deviceId = item->data(Qt::UserRole + 1).toInt();
    const auto params = item->data(Qt::UserRole).value<ConnectionDetails>();

    emit attemptToConnect(params, deviceId);
    close();
}

///
/// \brief DialogModbusScanner::on_lineEditIPAddressFrom_editingFinished
///
void DialogModbusScanner::on_lineEditIPAddressFrom_editingFinished()
{
    const auto address = ui->lineEditIPAddressFrom->value().toIPv4Address();
    if(address == 0) return;

    const auto mask = ui->lineEditSubnetMask->value().toIPv4Address();
    if(mask == 0) return;

    ui->lineEditIPAddressTo->setValue((address | ~mask) - 1);
}

///
/// \brief DialogModbusScanner::on_lineEditSubnetMask_editingFinished
///
void DialogModbusScanner::on_lineEditSubnetMask_editingFinished()
{
    const auto address = ui->lineEditIPAddressFrom->value().toIPv4Address();
    if(address == 0) return;

    const auto mask = ui->lineEditSubnetMask->value().toIPv4Address();
    if(mask == 0) return;

    ui->lineEditIPAddressFrom->setValue((address & mask) + 1);
    ui->lineEditIPAddressTo->setValue((address | ~mask) - 1);
}

///
/// \brief DialogModbusScanner::on_radioButtonRTU_clicked
///
void DialogModbusScanner::on_radioButtonRTU_clicked()
{
    ui->groupBoxIPAddressRange->setVisible(false);
    ui->groupBoxSerialPort->setVisible(true);
    ui->labelSpeed->setVisible(true);
    ui->labelDataBits->setVisible(true);
    ui->labelParity->setVisible(true);
    ui->labelStopBits->setVisible(true);
    ui->groupBoxPortRange->setVisible(false);
    ui->groupBoxSubnetMask->setVisible(false);
    ui->labelIPAddress->setVisible(false);
    ui->labelPort->setVisible(false);
    ui->labelScanResultsDesc->setText(tr("PORT: Device Id (serial port settings)"));
    ui->comboBoxFunction->setCurrentFunctionCode(_rtuFuncCode);
}

///
/// \brief DialogModbusScanner::on_radioButtonTCP_clicked
///
void DialogModbusScanner::on_radioButtonTCP_clicked()
{
    ui->groupBoxIPAddressRange->setVisible(true);
    ui->groupBoxPortRange->setVisible(true);
    ui->groupBoxSubnetMask->setVisible(true);
    ui->labelIPAddress->setVisible(true);
    ui->labelPort->setVisible(true);
    ui->groupBoxSerialPort->setVisible(false);
    ui->labelSpeed->setVisible(false);
    ui->labelDataBits->setVisible(false);
    ui->labelParity->setVisible(false);
    ui->labelStopBits->setVisible(false);
    ui->labelScanResultsDesc->setText(tr("Address: port (Device Id)"));
    ui->comboBoxFunction->setCurrentFunctionCode(_tcpFuncCode);
}

///
/// \brief DialogRtuScanner::startScan
///
void DialogModbusScanner::startScan()
{
    ScanParams params;
    if(ui->radioButtonRTU->isChecked())
    {
        params = createSerialParams();
        _scanner.reset(new ModbusRtuScanner(params, this));
    }
    else
    {
        params = createTcpParams();
        _scanner.reset(new ModbusTcpScanner(params, this));
    }

    if(params.ConnParams.empty())
    {
        stopScan();
        return;
    }

    connect(_scanner.get(), &ModbusScanner::timeout, this, &DialogModbusScanner::on_timeout);
    connect(_scanner.get(), &ModbusScanner::finished, this, &DialogModbusScanner::on_scanFinished, Qt::QueuedConnection);
    connect(_scanner.get(), &ModbusScanner::errorOccurred, this, &DialogModbusScanner::on_errorOccurred, Qt::QueuedConnection);
    connect(_scanner.get(), &ModbusScanner::found, this, &DialogModbusScanner::on_deviceFound, Qt::QueuedConnection);
    connect(_scanner.get(), &ModbusScanner::progress, this, &DialogModbusScanner::on_progress, Qt::QueuedConnection);

    clearScanTime();
    clearProgress();
    ui->progressBar->setValue(0);

    if(ui->listWidget->count() > 0)
    {
        const auto result = QMessageBox::question(this, windowTitle(), tr("Clear previous scan results?"));
        if(result == QMessageBox::Yes) ui->listWidget->clear();
    }

    _scanner->startScan();
    ui->pushButtonScan->setIcon(_iconStop);
}

///
/// \brief DialogRtuScanner::stopScan
///
void DialogModbusScanner::stopScan()
{
    if(_scanner)
    {
        _scanner->stopScan();
    }
}

///
/// \brief DialogRtuScanner::clearScanTime
///
void DialogModbusScanner::clearScanTime()
{
    on_timeout(0);
}

///
/// \brief DialogRtuScanner::clearProgress
///
void DialogModbusScanner::clearProgress()
{
    ui->labelSpeed->setText(QString(tr("Baud Rate:")));
    ui->labelDataBits->setText(QString(tr("Data Bits:")));
    ui->labelParity->setText(QString(tr("Parity:")));
    ui->labelStopBits->setText(QString(tr("Stop Bits:")));
    ui->labelIPAddress->setText(QString(tr("Address:")));
    ui->labelPort->setText(QString(tr("Port:")));
    ui->labelDeviceId->setText(QString(tr("Device Id:")));
}

///
/// \brief DialogModbusScanner::on_deviceFound
/// \param cd
/// \param deviceId
/// \param dubious
///
void DialogModbusScanner::on_deviceFound(const ConnectionDetails& cd, int deviceId, bool dubious)
{
    QString result;
    const auto id = QString("%1").arg(deviceId) + (dubious ? "?" : QString());
    if(ui->radioButtonRTU->isChecked())
    {
       result = QString("%1: %2 (%3,%4,%5,%6)").arg(cd.SerialParams.PortName,
                                                    id,
                                                    QString::number(cd.SerialParams.BaudRate),
                                                    QString::number(cd.SerialParams.WordLength),
                                                    Parity_toString(cd.SerialParams.Parity),
                                                    QString::number(cd.SerialParams.StopBits));
    }
    else
    {
       result = QString("%1: %2 (%3)").arg(cd.TcpParams.IPAddress,
                                           QString::number(cd.TcpParams.ServicePort),
                                           id);
    }

    QListWidgetItem* foundItem = nullptr;
    for(int i = 0; i < ui->listWidget->count(); ++i)
    {
       auto item = ui->listWidget->item(i);
       if(item &&
          item->data(Qt::UserRole).value<ConnectionDetails>() == cd &&
          item->data(Qt::UserRole + 1).toInt() == deviceId)
       {
            foundItem = item;
            break;
       }
    }

    if(!foundItem)
    {
       auto item = new QListWidgetItem(ui->listWidget);
       item->setText(result);
       item->setData(Qt::UserRole, QVariant::fromValue(cd));
       item->setData(Qt::UserRole + 1, deviceId);
       item->setData(Qt::UserRole + 2, dubious);

       ui->listWidget->addItem(item);
       ui->listWidget->scrollToItem(item);
    }
    else if(foundItem->data(Qt::UserRole + 2).toBool() && !dubious)
    {
        foundItem->setText(result);
    }
}

///
/// \brief DialogModbusScanner::on_progress
/// \param cd
/// \param deviceId
/// \param progress
///
void DialogModbusScanner::on_progress(const ConnectionDetails& cd, int deviceId, int progress)
{
    if(ui->radioButtonRTU->isChecked())
    {
        ui->labelSpeed->setText(QString(tr("Baud Rate: %1")).arg(cd.SerialParams.BaudRate));
        ui->labelDataBits->setText(QString(tr("Data Bits: %1")).arg(cd.SerialParams.WordLength));
        ui->labelParity->setText(QString(tr("Parity: %1")).arg(Parity_toString(cd.SerialParams.Parity)));
        ui->labelStopBits->setText(QString(tr("Stop Bits: %1")).arg(cd.SerialParams.StopBits));
    }
    else
    {
        ui->labelIPAddress->setText(QString(tr("Address: %1")).arg(cd.TcpParams.IPAddress));
        ui->labelPort->setText(QString(tr("Port: %1")).arg(cd.TcpParams.ServicePort));
    }

    ui->labelDeviceId->setText(QString(tr("Device Id: %1")).arg(deviceId));
    ui->progressBar->setValue(progress);
}

///
/// \brief DialogRtuScanner::createSerialParams
///
const ScanParams DialogModbusScanner::createSerialParams() const
{
    ScanParams params;

    QVector<QSerialPort::BaudRate> baudRates;
    for(auto&& obj : ui->groupBoxBaudRate->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            baudRates.push_back((QSerialPort::BaudRate)chbx->text().toInt());
    }

    QVector<QSerialPort::DataBits> dataBits;
    for(auto&& obj: ui->groupBoxDataBits->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            dataBits.push_back((QSerialPort::DataBits)chbx->text().toInt());
    }

    QVector<QSerialPort::Parity> parities;
    if(ui->checkBoxParityNone->isChecked())
        parities.push_back(QSerialPort::NoParity);
    if(ui->checkBoxParityOdd->isChecked())
        parities.push_back(QSerialPort::OddParity);
    if(ui->checkBoxParityEven->isChecked())
        parities.push_back(QSerialPort::EvenParity);

    QVector<QSerialPort::StopBits> stopBits;
    for(auto&& obj: ui->groupBoxStopBits->children())
    {
        auto chbx = qobject_cast<QCheckBox*>(obj);
        if(chbx && chbx->isChecked())
            stopBits.push_back((QSerialPort::StopBits)chbx->text().toInt());
    }

    for(auto&& baudRate : baudRates)
    {
        for(auto&& wordLength : dataBits)
        {
            for(auto&& parity : parities)
            {
                for(auto&& stop : stopBits)
                {
                    ConnectionDetails cd;
                    cd.Type = ConnectionType::Serial;
                    cd.SerialParams.PortName = ui->comboBoxSerial->currentText();
                    cd.SerialParams.BaudRate = baudRate;
                    cd.SerialParams.WordLength = wordLength;
                    cd.SerialParams.Parity = parity;
                    cd.SerialParams.StopBits = stop;
                    params.ConnParams.append(cd);
                }
            }
        }
    }

    // remove duplicates
    auto last = std::unique(params.ConnParams.begin(), params.ConnParams.end());
    params.ConnParams.erase(last, params.ConnParams.end());

    params.Request = createModbusRequest();
    params.Timeout = ui->spinBoxTimeout->value();
    params.RetryOnTimeout = ui->checkBoxRetryOnTimeout->isChecked();
    params.DeviceIds = QRange<int>(ui->spinBoxDeviceIdFrom->value(), ui->spinBoxDeviceIdTo->value());

    return params;
}

///
/// \brief DialogModbusScanner::createTcpParams
/// \return
///
const ScanParams DialogModbusScanner::createTcpParams() const
{
    ScanParams params;

    const auto mask = ui->lineEditSubnetMask->value().toIPv4Address();
    auto addressFrom = ui->lineEditIPAddressFrom->value().toIPv4Address();
    auto addressTo = ui->lineEditIPAddressTo->value().toIPv4Address();

    if(addressFrom == 0 || addressTo == 0)
        return params;

    if(addressTo < addressFrom)
    {
        std::swap(addressTo, addressFrom);
    }

    const auto addrRange = QRange<quint32>(addressFrom & mask, addressFrom | ~mask);
    if(!addrRange.contains(addressTo))
        return params;

    QVector<QString> addreses;
    for(quint32 addr = addressFrom; addr <= addressTo; addr++)
    {
        addreses.push_back(QHostAddress(addr).toString());
    }

    auto portFrom = ui->spinBoxPortFrom->value();
    auto portTo = ui->spinBoxPortTo->value();

    if(portTo < portFrom)
    {
        std::swap(portTo, portFrom);
    }

    QVector<int> ports;
    for(auto port = portFrom; port <= portTo; port++)
    {
        ports.push_back(port);
    }

    for(auto&& addr : addreses)
    {
        for(auto&& port : ports)
        {
            ConnectionDetails cd;
            cd.Type = ConnectionType::Tcp;
            cd.TcpParams.IPAddress = addr;
            cd.TcpParams.ServicePort = port;
            params.ConnParams.append(cd);
        }
    }

    params.Request = createModbusRequest();
    params.Timeout = ui->spinBoxTimeout->value();
    params.RetryOnTimeout = ui->checkBoxRetryOnTimeout->isChecked();
    params.DeviceIds = QRange<int>(ui->spinBoxDeviceIdFrom->value(), ui->spinBoxDeviceIdTo->value());

    return params;
}

///
/// \brief DialogModbusScanner::createModbusRequest
/// \return
///
const QModbusRequest DialogModbusScanner::createModbusRequest() const
{
    switch(ui->comboBoxFunction->currentFunctionCode())
    {
        case QModbusPdu::ReportServerId:
            return QModbusRequest(QModbusPdu::ReportServerId);

        case QModbusPdu::ReadCoils:
        case QModbusPdu::ReadDiscreteInputs:
        case QModbusPdu::ReadHoldingRegisters:
        case QModbusPdu::ReadInputRegisters:
        {
            const auto base = ui->comboBoxAddressBase->currentAddressBase();
            return QModbusRequest(ui->comboBoxFunction->currentFunctionCode(),
                              quint16(ui->spinBoxAddress->value() - (base == AddressBase::Base0 ? 0 : 1)),
                              quint16(ui->spinBoxLength->value()));
        }

        default:
            return QModbusRequest();
    }
}

///
/// \brief DialogModbusScanner::on_scanFinished
///
void DialogModbusScanner::on_scanFinished()
{
    ui->pushButtonScan->setIcon(_iconStart);
    clearProgress();
}

///
/// \brief DialogRtuScanner::on_timeout
/// \param time
///
void DialogModbusScanner::on_timeout(quint64 time)
{
    const auto str = QDateTime::fromSecsSinceEpoch(time).toUTC().toString("hh:mm:ss");
    ui->labelTimeLeft->setText(QString("<html><head/><body><p><span style=\"font-weight:700;\">%1</span></p></body></html>").arg(str));
}
