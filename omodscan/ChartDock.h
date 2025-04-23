#pragma once

#include <QDockWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegendMarker>
#include <QDateTime>
#include <QHash>

// QT_CHARTS_USE_NAMESPACE  // Qt5 下才需要此宏

    class ChartDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit ChartDock(QWidget *parent = nullptr);

public slots:
    void onRegisterValue(int addr, quint16 value);

private slots:
    void handleMarkerClicked();

private:
    QChart            *_chart;
    QChartView        *_view;
    QValueAxis        *_axisX;
    QValueAxis        *_axisY;
    QDateTime          _t0;
    QHash<int, QLineSeries*> _series;
    int                _currentAddr = -1;
};
