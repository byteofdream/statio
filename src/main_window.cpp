#include "statio/main_window.hpp"

#include "statio/system_info.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

QString formatBytes(std::uint64_t bytes) {
    constexpr double kb = 1024.0;
    constexpr double mb = kb * 1024.0;
    constexpr double gb = mb * 1024.0;
    constexpr double tb = gb * 1024.0;

    const double value = static_cast<double>(bytes);
    if (value >= tb) {
        return QString::number(value / tb, 'f', 2) + " TB";
    }
    if (value >= gb) {
        return QString::number(value / gb, 'f', 2) + " GB";
    }
    if (value >= mb) {
        return QString::number(value / mb, 'f', 2) + " MB";
    }
    if (value >= kb) {
        return QString::number(value / kb, 'f', 2) + " KB";
    }
    return QString::number(bytes) + " B";
}

QTableWidget* makeInfoTable(int columns, const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(parent);
    table->setColumnCount(columns);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->setAlternatingRowColors(true);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(false);
    return table;
}

void setCell(QTableWidget* table, int row, int col, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, col, item);
}

void setKeyValueRows(QTableWidget* table, const std::vector<std::pair<QString, QString>>& rows) {
    table->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        setCell(table, i, 0, rows[i].first);
        setCell(table, i, 1, rows[i].second);
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
}

QWidget* makeMetricCard(const QString& title, QLabel*& valueLabel, QWidget* parent) {
    auto* box = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(box);
    valueLabel = new QLabel("--", box);
    valueLabel->setObjectName("metricValue");
    valueLabel->setWordWrap(true);
    layout->addWidget(valueLabel);
    return box;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Statio");
    resize(1100, 760);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(16, 16, 16, 12);
    rootLayout->setSpacing(12);

    auto* topBar = new QHBoxLayout();
    auto* title = new QLabel("Statio System Inspector", central);
    title->setObjectName("titleLabel");

    refreshButton_ = new QPushButton("Refresh Now", central);
    topBar->addWidget(title);
    topBar->addStretch(1);
    topBar->addWidget(refreshButton_);

    tabs_ = new QTabWidget(central);
    setupTabs();

    statusLabel_ = new QLabel(central);
    statusLabel_->setObjectName("statusLabel");

    rootLayout->addLayout(topBar);
    rootLayout->addWidget(tabs_, 1);
    rootLayout->addWidget(statusLabel_);
    setCentralWidget(central);

    auto* helpMenu = menuBar()->addMenu("Help");
    auto* aboutAction = new QAction("About Statio", this);
    helpMenu->addAction(aboutAction);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    auto* settingsMenu = menuBar()->addMenu("Settings");
    auto* themeMenu = settingsMenu->addMenu("Theme");
    auto* actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    auto* lightAction = new QAction("Light", this);
    lightAction->setCheckable(true);
    lightAction->setChecked(true);
    auto* darkAction = new QAction("Dark", this);
    darkAction->setCheckable(true);

    actionGroup->addAction(lightAction);
    actionGroup->addAction(darkAction);
    themeMenu->addAction(lightAction);
    themeMenu->addAction(darkAction);

    connect(lightAction, &QAction::triggered, this, &MainWindow::setLightTheme);
    connect(darkAction, &QAction::triggered, this, &MainWindow::setDarkTheme);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(5000);

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshReport);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshReport);

    applyTheme(false);
    refreshReport();
    refreshTimer_->start();
}

void MainWindow::setupTabs() {
    tabs_->addTab(buildOverviewTab(), "Overview");
    tabs_->addTab(buildCpuTab(), "CPU");
    tabs_->addTab(buildMemoryTab(), "Memory");
    tabs_->addTab(buildDisksTab(), "Disks");
    tabs_->addTab(buildNetworkTab(), "Network");
    tabs_->addTab(buildGpuTab(), "GPU");
}

QWidget* MainWindow::buildOverviewTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(10);

    auto* hero = new QLabel("Realtime inventory of your system profile", page);
    hero->setObjectName("heroLabel");
    layout->addWidget(hero);

    auto* cards = new QGridLayout();
    cards->setHorizontalSpacing(10);
    cards->setVerticalSpacing(10);

    cards->addWidget(makeMetricCard("Host", overviewHostValue_, page), 0, 0);
    cards->addWidget(makeMetricCard("Operating System", overviewOsValue_, page), 0, 1);
    cards->addWidget(makeMetricCard("Kernel", overviewKernelValue_, page), 0, 2);
    cards->addWidget(makeMetricCard("CPU", overviewCpuValue_, page), 1, 0);
    cards->addWidget(makeMetricCard("Available RAM", overviewRamValue_, page), 1, 1);
    cards->addWidget(makeMetricCard("Disk Entries", overviewDiskCountValue_, page), 1, 2);
    cards->addWidget(makeMetricCard("Network Interfaces", overviewNetworkCountValue_, page), 2, 0);
    cards->addWidget(makeMetricCard("GPU Adapters", overviewGpuCountValue_, page), 2, 1);

    layout->addLayout(cards);
    layout->addStretch(1);

    return page;
}

QWidget* MainWindow::buildCpuTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    cpuTable_ = makeInfoTable(2, {"Metric", "Value"}, page);
    layout->addWidget(cpuTable_);
    return page;
}

QWidget* MainWindow::buildMemoryTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    memoryTable_ = makeInfoTable(2, {"Metric", "Value"}, page);
    layout->addWidget(memoryTable_);
    return page;
}

QWidget* MainWindow::buildDisksTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    diskTable_ = makeInfoTable(5, {"Mount", "FS", "Total", "Used", "Free"}, page);
    diskTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    diskTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(diskTable_);
    return page;
}

QWidget* MainWindow::buildNetworkTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    networkTable_ = makeInfoTable(5, {"Interface", "IPv4", "MAC", "RX", "TX"}, page);
    networkTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    layout->addWidget(networkTable_);
    return page;
}

QWidget* MainWindow::buildGpuTab() {
    auto* page = new QWidget(tabs_);
    auto* layout = new QVBoxLayout(page);
    gpuTable_ = makeInfoTable(2, {"Adapter", "Status"}, page);
    layout->addWidget(gpuTable_);
    return page;
}

void MainWindow::applyTheme(bool dark) {
    darkThemeEnabled_ = dark;

    if (!dark) {
        setStyleSheet(
            "QMainWindow {"
            "  background: #ffffff;"
            "  color: #000000;"
            "}"
            "QWidget {"
            "  color: #000000;"
            "}"
            "QLabel {"
            "  border: none;"
            "  background: transparent;"
            "}"
            "QMenuBar {"
            "  background: #ffffff;"
            "  color: #000000;"
            "  border: 2px solid #000000;"
            "  padding: 4px;"
            "}"
            "QMenuBar::item {"
            "  padding: 6px 10px;"
            "}"
            "QMenuBar::item:selected {"
            "  background: #e9e9e9;"
            "  color: #000000;"
            "}"
            "QMenu {"
            "  background: #ffffff;"
            "  color: #000000;"
            "  border: 2px solid #000000;"
            "}"
            "QTabWidget::pane {"
            "  border: 2px solid #000000;"
            "  background: #ffffff;"
            "  border-radius: 10px;"
            "}"
            "QTabBar::tab {"
            "  background: #f5f5f5;"
            "  color: #000000;"
            "  border: 2px solid #000000;"
            "  padding: 8px 14px;"
            "  margin-right: 4px;"
            "  border-top-left-radius: 8px;"
            "  border-top-right-radius: 8px;"
            "}"
            "QTabBar::tab:selected {"
            "  background: #ffffff;"
            "}"
            "QPushButton {"
            "  background: #ffffff;"
            "  color: #000000;"
            "  border: 2px solid #000000;"
            "  border-radius: 8px;"
            "  padding: 8px 14px;"
            "  font-weight: 400;"
            "}"
            "QPushButton:hover {"
            "  background: #efefef;"
            "}"
            "QPushButton:pressed {"
            "  background: #dcdcdc;"
            "}"
            "QGroupBox {"
            "  border: 2px solid #000000;"
            "  border-radius: 10px;"
            "  margin-top: 10px;"
            "  background: #ffffff;"
            "  font-weight: 400;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 12px;"
            "  padding: 0 5px;"
            "}"
            "QTableWidget {"
            "  background: #ffffff;"
            "  alternate-background-color: #f4f4f4;"
            "  border: 2px solid #000000;"
            "  border-radius: 10px;"
            "  gridline-color: transparent;"
            "  color: #000000;"
            "}"
            "QTableWidget::item {"
            "  border: none;"
            "}"
            "QHeaderView::section {"
            "  background: #f1f1f1;"
            "  color: #000000;"
            "  border: 1px solid #000000;"
            "  padding: 7px;"
            "  font-weight: 400;"
            "}"
            "QLabel#titleLabel {"
            "  font-size: 24px;"
            "  font-weight: 400;"
            "  color: #000000;"
            "}"
            "QLabel#heroLabel {"
            "  font-size: 14px;"
            "  color: #000000;"
            "  margin-bottom: 4px;"
            "}"
            "QLabel#metricValue {"
            "  font-size: 16px;"
            "  font-weight: 400;"
            "  color: #000000;"
            "}"
            "QLabel#statusLabel {"
            "  color: #000000;"
            "}"
        );
        return;
    }

    setStyleSheet(
        "QMainWindow {"
        "  background: #121212;"
        "  color: #f0f0f0;"
        "}"
        "QWidget {"
        "  color: #f0f0f0;"
        "}"
        "QLabel {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QMenuBar {"
        "  background: #1b1b1b;"
        "  color: #f0f0f0;"
        "  border: 2px solid #000000;"
        "  padding: 4px;"
        "}"
        "QMenuBar::item {"
        "  padding: 6px 10px;"
        "}"
        "QMenuBar::item:selected {"
        "  background: #303030;"
        "  color: #ffffff;"
        "}"
        "QMenu {"
        "  background: #1f1f1f;"
        "  color: #f0f0f0;"
        "  border: 2px solid #000000;"
        "}"
        "QTabWidget::pane {"
        "  border: 2px solid #000000;"
        "  background: #1a1a1a;"
        "  border-radius: 10px;"
        "}"
        "QTabBar::tab {"
        "  background: #2a2a2a;"
        "  color: #f0f0f0;"
        "  border: 2px solid #000000;"
        "  padding: 8px 14px;"
        "  margin-right: 4px;"
        "  border-top-left-radius: 8px;"
        "  border-top-right-radius: 8px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #3a3a3a;"
        "}"
        "QPushButton {"
        "  background: #2b2b2b;"
        "  color: #f0f0f0;"
        "  border: 2px solid #000000;"
        "  border-radius: 8px;"
        "  padding: 8px 14px;"
        "  font-weight: 400;"
        "}"
        "QPushButton:hover {"
        "  background: #3a3a3a;"
        "}"
        "QPushButton:pressed {"
        "  background: #242424;"
        "}"
        "QGroupBox {"
        "  border: 2px solid #000000;"
        "  border-radius: 10px;"
        "  margin-top: 10px;"
        "  background: #1f1f1f;"
        "  font-weight: 400;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 5px;"
        "}"
        "QTableWidget {"
        "  background: #1f1f1f;"
        "  alternate-background-color: #252525;"
        "  border: 2px solid #000000;"
        "  border-radius: 10px;"
        "  gridline-color: transparent;"
        "  color: #f0f0f0;"
        "}"
        "QTableWidget::item {"
        "  border: none;"
        "}"
        "QHeaderView::section {"
        "  background: #2b2b2b;"
        "  color: #f0f0f0;"
        "  border: 1px solid #000000;"
        "  padding: 7px;"
        "  font-weight: 400;"
        "}"
        "QLabel#titleLabel {"
        "  font-size: 24px;"
        "  font-weight: 400;"
        "  color: #ffffff;"
        "}"
        "QLabel#heroLabel {"
        "  font-size: 14px;"
        "  color: #f0f0f0;"
        "  margin-bottom: 4px;"
        "}"
        "QLabel#metricValue {"
        "  font-size: 16px;"
        "  font-weight: 400;"
        "  color: #ffffff;"
        "}"
        "QLabel#statusLabel {"
        "  color: #f0f0f0;"
        "}"
    );
}

void MainWindow::refreshReport() {
    const auto snapshot = statio::collectSystemSnapshot();
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    overviewHostValue_->setText(QString::fromStdString(snapshot.os.hostname.empty() ? "N/A" : snapshot.os.hostname));
    overviewOsValue_->setText(QString::fromStdString(snapshot.os.distro.empty() ? "N/A" : snapshot.os.distro));
    overviewKernelValue_->setText(QString::fromStdString(snapshot.os.kernel.empty() ? "N/A" : snapshot.os.kernel));
    overviewCpuValue_->setText(QString::fromStdString(snapshot.cpu.model.empty() ? "N/A" : snapshot.cpu.model));
    overviewRamValue_->setText(QString::number(snapshot.memory.availableMB) + " MB");
    overviewDiskCountValue_->setText(QString::number(static_cast<int>(snapshot.disks.size())));
    overviewNetworkCountValue_->setText(QString::number(static_cast<int>(snapshot.network.size())));
    overviewGpuCountValue_->setText(QString::number(static_cast<int>(snapshot.gpus.size())));

    setKeyValueRows(cpuTable_, {
                                {"Model", QString::fromStdString(snapshot.cpu.model.empty() ? "N/A" : snapshot.cpu.model)},
                                {"Physical cores", QString::number(snapshot.cpu.physicalCores)},
                                {"Logical threads", QString::number(snapshot.cpu.logicalThreads)},
                                {"Current MHz", QString::number(snapshot.cpu.currentMHz, 'f', 2)},
                            });

    setKeyValueRows(memoryTable_, {
                                   {"Total RAM", QString::number(snapshot.memory.totalMB) + " MB"},
                                   {"Free RAM", QString::number(snapshot.memory.freeMB) + " MB"},
                                   {"Available RAM", QString::number(snapshot.memory.availableMB) + " MB"},
                                   {"Total Swap", QString::number(snapshot.memory.swapTotalMB) + " MB"},
                                   {"Free Swap", QString::number(snapshot.memory.swapFreeMB) + " MB"},
                               });

    diskTable_->setRowCount(static_cast<int>(snapshot.disks.size()));
    for (int i = 0; i < static_cast<int>(snapshot.disks.size()); ++i) {
        const auto& disk = snapshot.disks[static_cast<std::size_t>(i)];
        const auto usedGB = disk.totalGB >= disk.freeGB ? (disk.totalGB - disk.freeGB) : 0ULL;

        setCell(diskTable_, i, 0, QString::fromStdString(disk.mountPoint));
        setCell(diskTable_, i, 1, QString::fromStdString(disk.filesystem));
        setCell(diskTable_, i, 2, QString::number(disk.totalGB) + " GB");
        setCell(diskTable_, i, 3, QString::number(usedGB) + " GB");
        setCell(diskTable_, i, 4, QString::number(disk.freeGB) + " GB");
    }
    diskTable_->resizeColumnsToContents();
    diskTable_->horizontalHeader()->setStretchLastSection(true);

    networkTable_->setRowCount(static_cast<int>(snapshot.network.size()));
    for (int i = 0; i < static_cast<int>(snapshot.network.size()); ++i) {
        const auto& net = snapshot.network[static_cast<std::size_t>(i)];
        setCell(networkTable_, i, 0, QString::fromStdString(net.name));
        setCell(networkTable_, i, 1, net.ipv4.empty() ? "N/A" : QString::fromStdString(net.ipv4));
        setCell(networkTable_, i, 2, net.mac.empty() ? "N/A" : QString::fromStdString(net.mac));
        setCell(networkTable_, i, 3, formatBytes(net.rxBytes));
        setCell(networkTable_, i, 4, formatBytes(net.txBytes));
    }
    networkTable_->resizeColumnsToContents();
    networkTable_->horizontalHeader()->setStretchLastSection(true);

    gpuTable_->setRowCount(static_cast<int>(snapshot.gpus.size()));
    for (int i = 0; i < static_cast<int>(snapshot.gpus.size()); ++i) {
        const auto& gpu = snapshot.gpus[static_cast<std::size_t>(i)];
        setCell(gpuTable_, i, 0, QString::fromStdString(gpu.adapter));
        setCell(gpuTable_, i, 1, gpu.detected ? "Detected" : "Fallback");
    }
    gpuTable_->resizeColumnsToContents();
    gpuTable_->horizontalHeader()->setStretchLastSection(true);

    statusLabel_->setText("Last update: " + stamp + " | Auto-refresh: 5s");
}

void MainWindow::showAboutDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("About Statio");
    dialog.setMinimumSize(560, 420);
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #ffffff; border: 2px solid #000000; }"
        "QLabel { color: #000000; border: none; background: transparent; }"
        "QPushButton {"
        "  background: #ffffff;"
        "  color: #000000;"
        "  border: 2px solid #000000;"
        "  border-radius: 8px;"
        "  padding: 8px 20px;"
        "  font-weight: 400;"
        "}"
        "QPushButton:hover { background: #efefef; }"
    );

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("Statio", &dialog);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 34px; font-weight: 400;");

    auto* subtitle = new QLabel("System Diagnostics Toolkit", &dialog);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 16px; font-weight: 400;");

    auto* body = new QLabel(
        "Built in C++ and Qt to inspect hardware and operating-system details in one place.",
        &dialog);
    body->setAlignment(Qt::AlignCenter);
    body->setWordWrap(true);
    body->setStyleSheet("font-size: 14px; font-weight: 400;");

    auto* version = new QLabel("Version 0.1", &dialog);
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("font-size: 13px; font-weight: 400;");

    auto* features = new QLabel(
        "Features\n"
        "OS, CPU, Memory, Disks, Network, GPU\n"
        "Tabbed dashboard with auto-refresh\n"
        "CLI + Qt GUI modes",
        &dialog);
    features->setAlignment(Qt::AlignCenter);
    features->setWordWrap(true);
    features->setStyleSheet("font-size: 14px; font-weight: 400;");

    auto* footer = new QLabel("Statio Project", &dialog);
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet("font-size: 12px; font-weight: 400;");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    buttons->setCenterButtons(true);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(4);
    layout->addWidget(body);
    layout->addWidget(version);
    layout->addWidget(features);
    layout->addWidget(footer);
    layout->addSpacing(4);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::setLightTheme() {
    applyTheme(false);
}

void MainWindow::setDarkTheme() {
    applyTheme(true);
}
