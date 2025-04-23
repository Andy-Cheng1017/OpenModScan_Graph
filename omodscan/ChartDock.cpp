#include "ChartDock.h"

ChartDock::ChartDock(QWidget *parent)
    : QDockWidget(tr("Live Trend"), parent),
    _chart(new QChart),
    _view(new QChartView(_chart)),
    _axisX(new QValueAxis),
    _axisY(new QValueAxis),
    _t0(QDateTime::currentDateTime())
{
    setWidget(_view);
    _chart->setTitle("Register Trend");
    // 把图例放上面一行，方便点
    _chart->legend()->setVisible(true);
    _chart->legend()->setAlignment(Qt::AlignTop);

    // X 轴：0…60 秒
    _axisX->setTitleText("Time (s)");
    _axisX->setRange(0, 300);
    _chart->addAxis(_axisX, Qt::AlignBottom);

    // Y 轴：200…1000（固定）
    _axisY->setTitleText("Value");
    _axisY->setRange(200, 1000);
    _chart->addAxis(_axisY, Qt::AlignLeft);
}

void ChartDock::onRegisterValue(int addr, quint16 value)
{
    // 如果之前没点选过，先不画任何曲线
    if (_currentAddr >= 0 && addr != _currentAddr)
        return;

    // 第一次收到这个地址的数据，就建这条曲线
    if (!_series.contains(addr)) {
        auto *s = new QLineSeries;
        s->setName(QString("R%1").arg(addr + 1));
        _chart->addSeries(s);
        s->attachAxis(_axisX);
        s->attachAxis(_axisY);
        _series.insert(addr, s);

        // 让 legend 标记可点击
        for (auto *marker : _chart->legend()->markers(s)) {
            connect(marker, &QLegendMarker::clicked,
                    this, &ChartDock::handleMarkerClicked);
        }
    }

    // 时间 X / 值 Y（假设 value 本身就在 200…1000）
    qreal x = _t0.msecsTo(QDateTime::currentDateTime()) / 1000.0;
    qreal y = value;
    auto *s = _series.value(addr);
    s->append(x, y);

    // 滑动窗口：保留最近 500 点
    constexpr int MaxPoints = 800;
    if (s->count() > MaxPoints)
        s->removePoints(0, s->count() - MaxPoints);

    // 滚动 X 轴
    if (x > 300)
        _axisX->setRange(x - 300, x);
}

void ChartDock::handleMarkerClicked()
{
    // 找到被点击的 marker
    auto *marker = qobject_cast<QLegendMarker*>(sender());
    if (!marker) return;

    // 找到这个 marker 对应的 series 地址
    int clickedAddr = -1;
    for (auto it = _series.begin(); it != _series.end(); ++it) {
        if (it.value() == marker->series()) {
            clickedAddr = it.key();
            break;
        }
    }
    if (clickedAddr < 0) return;

    // 隐藏所有 series，只有被点击的那条显示
    for (auto it = _series.begin(); it != _series.end(); ++it) {
        bool show = (it.key() == clickedAddr);
        it.value()->setVisible(show);
    }
    _currentAddr = clickedAddr;
}
