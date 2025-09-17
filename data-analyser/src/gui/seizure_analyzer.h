#ifndef SEIZURE_ANALYZER_H
#define SEIZURE_ANALYZER_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QDate>
#include <QFileSystemWatcher>
#include <QComboBox>
#include "../core/hdf5_reader.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
QT_END_NAMESPACE

struct SeizureDetection {
    QDateTime timestamp;
    QString type;
    double confidence;
    double activityLevel;
    int rawData;
    QString filePath;
    int channelIndex; 
    // Channel where detection occurred (0-31)
};

class SeizureAnalyzer : public QMainWindow
{
    Q_OBJECT

public:
    SeizureAnalyzer(QWidget *parent = nullptr);
    ~SeizureAnalyzer();

private slots:
    void reloadData();
    void updateDisplay();
    void onFileChanged(const QString &path);
    void onChannelChanged(int channel);

private:
    void setupUI();
    void scanLogFiles();
    void parseHdf5File(const QString &filePath);
    void updateSeizureCounts();
    void updateLatestDetections();
    void updateDailyCounts();
    void updateChannelData();
    
    // UI Components
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;
    QGridLayout *statsLayout;
    QGridLayout *dailyLayout;
    
    // Channel selection
    QComboBox *channelSelector;
    QLabel *channelInfoLabel;
    
    QPushButton *reloadButton;
    QLabel *totalSeizuresLabel;
    QLabel *todaySeizuresLabel;
    QLabel *monthlySeizuresLabel;
    QLabel *lastUpdateLabel;
    
    QTableWidget *latestDetectionsTable;
    QTableWidget *dailyCountsTable;
    QTableWidget *channelDataTable;
    
    // Data
    QList<SeizureDetection> allDetections;
    QMap<QDate, int> dailyCounts;
    QMap<QString, int> monthlyCounts;
    QFileSystemWatcher *fileWatcher;
    QTimer *updateTimer;
    
    QString logsDirectory;
    int selectedChannel;
};

#endif // SEIZURE_ANALYZER_H
