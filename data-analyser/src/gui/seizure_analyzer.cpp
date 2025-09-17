#include "seizure_analyzer.h"
#include "../core/hdf5_reader.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <QTextStream>
#include <QRegularExpression>
#include <chrono>

SeizureAnalyzer::SeizureAnalyzer(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget(nullptr)
    , fileWatcher(nullptr)
    , updateTimer(nullptr)
    , selectedChannel(0)
{
    // Get the directory where the executable is located
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check if we're running from the build directory or from data-analyser directory
    if (appDir.contains("build/seizure_analyzer.app/Contents/MacOS")) {
        logsDirectory = QDir(appDir).absoluteFilePath("../../../../logs");
    } else {
        logsDirectory = QDir(appDir).absoluteFilePath("logs");
    }
    
    setupUI();
    scanLogFiles();
    
    // Set up file watcher
    fileWatcher = new QFileSystemWatcher(this);
    fileWatcher->addPath(logsDirectory);
    connect(fileWatcher, &QFileSystemWatcher::directoryChanged, this, &SeizureAnalyzer::onFileChanged);
    
    // Set up update timer (check every 5 seconds)
    
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &SeizureAnalyzer::updateDisplay);
    updateTimer->start(5000);
    
    setWindowTitle("Seizure Detection Analyzer");
    setMinimumSize(800, 600);
}

SeizureAnalyzer::~SeizureAnalyzer()
{
}

void SeizureAnalyzer::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    
    // Button layout
    buttonLayout = new QHBoxLayout();
    reloadButton = new QPushButton("Reload Data", this);
    connect(reloadButton, &QPushButton::clicked, this, &SeizureAnalyzer::reloadData);
    buttonLayout->addWidget(reloadButton);
    
    // Channel selector
    buttonLayout->addWidget(new QLabel("Channel:", this));
    channelSelector = new QComboBox(this);
    for (int i = 0; i < 32; ++i) {
        QString channelName = QString("A-%1").arg(i, 3, 10, QChar('0')); // A-000, A-001, etc.
        channelSelector->addItem(channelName);
    }
    connect(channelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SeizureAnalyzer::onChannelChanged);
    buttonLayout->addWidget(channelSelector);
    
    // Channel info label
    channelInfoLabel = new QLabel("", this);
    buttonLayout->addWidget(channelInfoLabel);
    
    buttonLayout->addStretch();
    
    // Stats layout
    statsLayout = new QGridLayout();
    totalSeizuresLabel = new QLabel("Total Seizures: 0", this);
    todaySeizuresLabel = new QLabel("Today: 0", this);
    monthlySeizuresLabel = new QLabel("This Month: 0", this);
    lastUpdateLabel = new QLabel("Last Update: Never", this);
    
    totalSeizuresLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    todaySeizuresLabel->setStyleSheet("font-size: 14px;");
    monthlySeizuresLabel->setStyleSheet("font-size: 14px;");
    lastUpdateLabel->setStyleSheet("font-size: 12px; color: gray;");
    
    statsLayout->addWidget(totalSeizuresLabel, 0, 0);
    statsLayout->addWidget(todaySeizuresLabel, 0, 1);
    statsLayout->addWidget(monthlySeizuresLabel, 0, 2);
    statsLayout->addWidget(lastUpdateLabel, 0, 3);
    
    // Latest detections table
    QLabel *latestLabel = new QLabel("Latest 20 Detections:", this);
    latestLabel->setStyleSheet("font-weight: bold;");
    
    latestDetectionsTable = new QTableWidget(20, 5, this);
    latestDetectionsTable->setHorizontalHeaderLabels({"Timestamp", "Type", "Confidence", "Activity Level", "File"});
    latestDetectionsTable->horizontalHeader()->setStretchLastSection(true);
    latestDetectionsTable->setAlternatingRowColors(true);
    latestDetectionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    // Daily counts table
    QLabel *dailyLabel = new QLabel("Daily Counts:", this);
    dailyLabel->setStyleSheet("font-weight: bold;");
    
    dailyCountsTable = new QTableWidget(0, 2, this);
    dailyCountsTable->setHorizontalHeaderLabels({"Date", "Seizure Count"});
    dailyCountsTable->horizontalHeader()->setStretchLastSection(true);
    dailyCountsTable->setAlternatingRowColors(true);
    
    // Channel data table
    QLabel *channelLabel = new QLabel("Channel Data:", this);
    channelLabel->setStyleSheet("font-weight: bold;");
    
    channelDataTable = new QTableWidget(0, 3, this);
    channelDataTable->setHorizontalHeaderLabels({"Timestamp", "Value (μV)", "File"});
    channelDataTable->horizontalHeader()->setStretchLastSection(true);
    channelDataTable->setAlternatingRowColors(true);
    channelDataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    // Add to main layout
    mainLayout->addLayout(buttonLayout);
    mainLayout->addLayout(statsLayout);
    mainLayout->addWidget(latestLabel);
    mainLayout->addWidget(latestDetectionsTable);
    mainLayout->addWidget(dailyLabel);
    mainLayout->addWidget(dailyCountsTable);
    mainLayout->addWidget(channelLabel);
    mainLayout->addWidget(channelDataTable);
}

void SeizureAnalyzer::reloadData()
{
    scanLogFiles();
    updateDisplay();
}

void SeizureAnalyzer::updateDisplay()
{
    updateSeizureCounts();
    updateLatestDetections();
    updateDailyCounts();
    updateChannelData();
    lastUpdateLabel->setText("Last Update: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
}

void SeizureAnalyzer::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    // File system changed, reload data
    reloadData();
}

void SeizureAnalyzer::scanLogFiles()
{
    allDetections.clear();
    dailyCounts.clear();
    monthlyCounts.clear();
    
    // Debug: Print current working directory and logs directory
    QString currentDir = QDir::currentPath();
    QString absoluteLogsDir = QDir(logsDirectory).absolutePath();
    
    QDir logsDir(logsDirectory);
    if (!logsDir.exists()) {
        QString errorMsg = QString("Logs directory not found!\n"
                                  "Current directory: %1\n"
                                  "Looking for: %2\n"
                                  "Absolute path: %3")
                          .arg(currentDir)
                          .arg(logsDirectory)
                          .arg(absoluteLogsDir);
        QMessageBox::warning(this, "Warning", errorMsg);
        return;
    }
    
    // Scan all date directories (no debug output for long-term stability)
    QStringList dateDirs = logsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dateDir : dateDirs) {
        QDir dayDir(logsDir.absoluteFilePath(dateDir));
        QStringList h5Files = dayDir.entryList(QStringList() << "*.h5", QDir::Files);
        
        for (const QString &h5File : h5Files) {
            QString filePath = dayDir.absoluteFilePath(h5File);
            parseHdf5File(filePath);
        }
    }
}

void SeizureAnalyzer::parseHdf5File(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    
    // Check if this is an hourly FPGA response file
    if (fileName.startsWith("hour_") && fileName.endsWith(".h5")) {
        // Extract hour from filename
        QRegularExpression hourRegex("hour_(\\d+)\\.h5");
        QRegularExpressionMatch match = hourRegex.match(fileName);
        if (match.hasMatch()) {
            // Extract hour from filename (for potential future use)
            int hour = match.captured(1).toInt();
            Q_UNUSED(hour); // Suppress unused variable warning
            
            // Extract date from directory name
            QString dateStr = fileInfo.dir().dirName();
            QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
            
            if (date.isValid()) {
                // Parse the actual HDF5 file
                Hdf5Reader reader;
                if (reader.open(filePath.toStdString())) {
                    std::vector<SeizureDetectionData> detections = reader.readSeizureDetections();
                    
                    for (const auto& detection : detections) {
                        // Only add seizure detections (not normal activity)
                        QString detectionType = QString::fromStdString(detection.responseType);
                        if (detectionType == "SEIZURE_DETECTED" || detectionType == "THRESHOLD_EXCEEDED") {
                            SeizureDetection qtDetection;
                            
                            // Convert std::chrono::time_point to QDateTime
                            auto time_t = std::chrono::system_clock::to_time_t(detection.timestamp);
                            qtDetection.timestamp = QDateTime::fromSecsSinceEpoch(time_t);
                            
                            qtDetection.type = detectionType;
                            qtDetection.confidence = detection.confidence;
                            qtDetection.activityLevel = detection.activityLevel;
                            qtDetection.rawData = detection.rawData;
                            qtDetection.filePath = filePath;
                            qtDetection.channelIndex = detection.channelIndex;
                            
                            allDetections.append(qtDetection);
                        }
                    }
                    
                    reader.close();
                } else {
                    qDebug() << "Failed to open HDF5 file:" << filePath;
                }
            }
        }
    }
    
    // Also check for neural data files (intan_shm_*.h5)
    if (fileName.startsWith("intan_shm_") && fileName.endsWith(".h5")) {
        // For neural data files, we could parse them too if needed
        // For now, just count them as potential seizure sources
    }
}

void SeizureAnalyzer::updateSeizureCounts()
{
    // Filter detections by selected channel
    QList<SeizureDetection> channelDetections;
    for (const SeizureDetection &detection : allDetections) {
        if (detection.channelIndex == selectedChannel) {
            channelDetections.append(detection);
        }
    }
    
    int totalSeizures = channelDetections.size();
    int todaySeizures = 0;
    int monthlySeizures = 0;
    
    QDate today = QDate::currentDate();
    QString currentMonth = today.toString("yyyy-MM");
    
    for (const SeizureDetection &detection : channelDetections) {
        if (detection.timestamp.date() == today) {
            todaySeizures++;
        }
        
        QString detectionMonth = detection.timestamp.date().toString("yyyy-MM");
        if (detectionMonth == currentMonth) {
            monthlySeizures++;
        }
    }
    
    totalSeizuresLabel->setText(QString("Total Seizures: %1").arg(totalSeizures));
    todaySeizuresLabel->setText(QString("Today: %1").arg(todaySeizures));
    monthlySeizuresLabel->setText(QString("This Month: %1").arg(monthlySeizures));
}

void SeizureAnalyzer::updateLatestDetections()
{
    // Filter detections by selected channel
    QList<SeizureDetection> channelDetections;
    for (const SeizureDetection &detection : allDetections) {
        if (detection.channelIndex == selectedChannel) {
            channelDetections.append(detection);
        }
    }
    
    // Sort detections by timestamp (newest first)
    std::sort(channelDetections.begin(), channelDetections.end(), 
              [](const SeizureDetection &a, const SeizureDetection &b) {
                  return a.timestamp > b.timestamp;
              });
    
    // Take latest 20
    int count = qMin(20, channelDetections.size());
    latestDetectionsTable->setRowCount(count);
    
    for (int i = 0; i < count; ++i) {
        const SeizureDetection &detection = channelDetections[i];
        
        latestDetectionsTable->setItem(i, 0, new QTableWidgetItem(detection.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
        latestDetectionsTable->setItem(i, 1, new QTableWidgetItem(detection.type));
        latestDetectionsTable->setItem(i, 2, new QTableWidgetItem(QString::number(detection.confidence, 'f', 3)));
        latestDetectionsTable->setItem(i, 3, new QTableWidgetItem(QString::number(detection.activityLevel, 'f', 3)));
        latestDetectionsTable->setItem(i, 4, new QTableWidgetItem(QFileInfo(detection.filePath).fileName()));
    }
}

void SeizureAnalyzer::updateDailyCounts()
{
    // Count seizures per day for selected channel
    dailyCounts.clear();
    for (const SeizureDetection &detection : allDetections) {
        if (detection.channelIndex == selectedChannel) {
            QDate date = detection.timestamp.date();
            dailyCounts[date]++;
        }
    }
    
    // Update table
    dailyCountsTable->setRowCount(dailyCounts.size());
    
    QList<QDate> dates = dailyCounts.keys();
    std::sort(dates.begin(), dates.end(), std::greater<QDate>());
    
    for (int i = 0; i < dates.size(); ++i) {
        QDate date = dates[i];
        int count = dailyCounts[date];
        
        dailyCountsTable->setItem(i, 0, new QTableWidgetItem(date.toString("yyyy-MM-dd")));
        dailyCountsTable->setItem(i, 1, new QTableWidgetItem(QString::number(count)));
    }
}

void SeizureAnalyzer::onChannelChanged(int channel)
{
    selectedChannel = channel;
    
    // Update channel info label
    QString channelInfo = QString("Wire Channel A-%1 - Neural data from electrode").arg(channel, 3, 10, QChar('0'));
    channelInfoLabel->setText(channelInfo);
    
    // Update display to show data for selected channel
    updateDisplay();
}

void SeizureAnalyzer::updateChannelData()
{
    channelDataTable->setRowCount(0);
    
    if (allDetections.isEmpty()) {
        return;
    }
    
    // Get the most recent HDF5 file to read channel data from
    QString latestFile;
    QDateTime latestTime;
    
    for (const SeizureDetection &detection : allDetections) {
        if (detection.timestamp > latestTime) {
            latestTime = detection.timestamp;
            latestFile = detection.filePath;
        }
    }
    
    if (latestFile.isEmpty()) {
        return;
    }
    
    // Read channel data from the latest file
    Hdf5Reader reader;
    if (!reader.open(latestFile.toStdString())) {
        return;
    }
    
    std::vector<float> channelData = reader.readChannelData(selectedChannel);
    reader.close();
    
    if (channelData.empty()) {
        return;
    }
    
    // Show the last 50 data points
    int numPoints = qMin(50, static_cast<int>(channelData.size()));
    channelDataTable->setRowCount(numPoints);
    
    for (int i = 0; i < numPoints; ++i) {
        int dataIndex = channelData.size() - numPoints + i;
        float value = channelData[dataIndex];
        
        // Generate timestamp (approximate based on data index)
        QDateTime timestamp = latestTime.addSecs(dataIndex);
        
        channelDataTable->setItem(i, 0, new QTableWidgetItem(timestamp.toString("hh:mm:ss")));
        channelDataTable->setItem(i, 1, new QTableWidgetItem(QString::number(value, 'f', 3)));
        channelDataTable->setItem(i, 2, new QTableWidgetItem(QFileInfo(latestFile).fileName()));
    }
}
