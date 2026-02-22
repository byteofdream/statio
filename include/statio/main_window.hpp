#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void refreshReport();
    void showAboutDialog();
    void setLightTheme();
    void setDarkTheme();

private:
    void setupTabs();
    QWidget* buildOverviewTab();
    QWidget* buildCpuTab();
    QWidget* buildMemoryTab();
    QWidget* buildDisksTab();
    QWidget* buildNetworkTab();
    QWidget* buildGpuTab();
    void applyTheme(bool dark);

    QTabWidget* tabs_ = nullptr;

    QLabel* overviewHostValue_ = nullptr;
    QLabel* overviewOsValue_ = nullptr;
    QLabel* overviewKernelValue_ = nullptr;
    QLabel* overviewCpuValue_ = nullptr;
    QLabel* overviewRamValue_ = nullptr;
    QLabel* overviewDiskCountValue_ = nullptr;
    QLabel* overviewNetworkCountValue_ = nullptr;
    QLabel* overviewGpuCountValue_ = nullptr;

    QTableWidget* cpuTable_ = nullptr;
    QTableWidget* memoryTable_ = nullptr;
    QTableWidget* diskTable_ = nullptr;
    QTableWidget* networkTable_ = nullptr;
    QTableWidget* gpuTable_ = nullptr;

    QLabel* statusLabel_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    bool darkThemeEnabled_ = false;
};
