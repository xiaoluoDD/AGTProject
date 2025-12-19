#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QLabel>
#include <QLoggingCategory>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QStandardPaths>
#include <QMap>

// 前向声明
class QTableWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class tcpClient;
}
QT_END_NAMESPACE

/**
 * @brief TCP客户端主窗口类
 *
 * 提供TCP连接管理、数据发送接收、16进制数据处理等功能
 */
class tcpClient : public QWidget
{
    Q_OBJECT

public:
    tcpClient(QWidget *parent = nullptr);
    ~tcpClient();

signals:
    // 数据解析完成信号
    void dataParsed(int status1, unsigned int value1, unsigned int value2,
                    int status2, unsigned int value3, unsigned int value4,
                    const QString &currentTime);

private slots:
    // 界面切换槽函数
    void onConnectionPageClicked(); ///< 切换到连接界面
    void onTablePageClicked();      ///< 切换到表格界面
    void onVehicleBindingPageClicked(); ///< 切换到车型绑定界面

    // 连接管理槽函数
    void onConnectClicked();      ///< 连接按钮点击处理
    void onDisconnectClicked();   ///< 断开按钮点击处理
    void onSendClicked();         ///< 发送按钮点击处理
    void onClearClicked();        ///< 清空日志按钮点击处理

    // 表格操作槽函数
    void onClearTableClicked();   ///< 清空表格按钮点击处理
    void onDeleteTableClicked();  ///< 删除选中表格按钮点击处理
    void onExportTableClicked();  ///< 导出表格按钮点击处理

    // 车型绑定表格操作槽函数
    void onAddVehicleClicked();   ///< 添加车型按钮点击处理
    void onDeleteVehicleClicked(); ///< 删除选中车型按钮点击处理
    void onClearVehicleTableClicked(); ///< 清空车型表格按钮点击处理
    void onExportVehicleTableClicked(); ///< 导出车型表格按钮点击处理
    void onVehicleBindingItemChanged(QTableWidgetItem *item); ///< 车型绑定表格数据变化处理

    // 数据解析槽函数
    void onDataParsed(int status1, unsigned int value1, unsigned int value2,
                      int status2, unsigned int value3, unsigned int value4,
                      const QString &currentTime); ///< 处理解析完成的数据

    // 网络事件槽函数
    void onSocketConnected();     ///< Socket连接成功处理
    void onSocketDisconnected();  ///< Socket断开连接处理
    void onSocketError(QAbstractSocket::SocketError error); ///< Socket错误处理
    void onSocketReadyRead();     ///< Socket数据可读处理
    void onConnectionTimeout();   ///< 连接超时处理

    // 密码管理槽函数
    void onSetPasswordClicked();  ///< 设置密码按钮点击处理
    
    // 数据库配置槽函数
    void onSaveDatabaseConfigClicked(); ///< 保存数据库配置按钮点击处理
    void onTestDatabaseConnectionClicked(); ///< 测试数据库连接按钮点击处理

private:
    // UI和状态管理
    void setupUI();               ///< 初始化用户界面
    void setupTable();            ///< 初始化表格
    void setupVehicleBindingTable(); ///< 初始化车型绑定表格
    void updateConnectionStatus(bool connected); ///< 更新连接状态显示
    void appendToLog(const QString &message, bool isError = false); ///< 添加日志信息
    void applyModernStyle();      ///< 应用现代化样式
    void applyBuiltinStyle();     ///< 应用内置样式

    // 数据处理
    void sendData(const QByteArray &data); ///< 发送数据到服务器
    void processHexData(const QByteArray &data); ///< 处理16进制数据
    void addDataToTable(int status1, unsigned int value1, unsigned int value2,
                        int status2, unsigned int value3, unsigned int value4,
                        const QString &currentTime); ///< 添加数据到表格

    void initDatabase();
    void loadDatabaseConfig(); ///< 从配置文件加载数据库配置
    void saveDatabaseConfig(); ///< 保存数据库配置到配置文件
    bool testDatabaseConnection(const QString &host, int port, const QString &database, 
                                const QString &username, const QString &password); ///< 测试数据库连接
    void loadModelBindingsFromDb();
    void loadDataRecordsFromDb();
    void insertDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime);
    void deleteDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime);
    void insertModelBinding(const QString &modelCode, const QString &modelName, int count);
    void updateModelBinding(const QString &oldModelCode, const QString &oldModelName, int oldCount, int row);
    void deleteModelBinding(const QString &modelName);
    void clearModelBindings();
    void clearDataRecords();

    // 密码管理函数
    void initPasswordTable();     ///< 初始化密码表
    void savePassword(const QString &password); ///< 保存密码到数据库
    QString loadPassword();       ///< 从数据库加载密码
    bool verifyPassword(const QString &inputPassword); ///< 验证密码
    bool showPasswordDialog(const QString &title, const QString &message); ///< 显示密码输入对话框
    void updatePasswordDisplay(); ///< 更新密码显示

    // 日志系统函数
    void initLogSystem();         ///< 初始化日志系统
    void setupLogDirectory();     ///< 设置日志目录
    void setupLogFile();          ///< 设置日志文件
    void cleanupLogFiles();       ///< 清理旧日志文件
    static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg); ///< 日志消息处理器

    // 事件处理
    void closeEvent(QCloseEvent *event) override; ///< 重写关闭事件

private:
    Ui::tcpClient *ui;            ///< UI界面指针
    QTcpSocket *m_socket;         ///< TCP Socket对象
    QTimer *m_connectionTimer;    ///< 连接超时定时器
    bool m_isConnected;           ///< 连接状态标志
    int m_fullTrayCount;          ///< 满托盘时数量
    QDateTime m_lastStatus1Time;  ///< 旧版本兼容，已废弃
    QDateTime m_lastStatus2Time; ///< 旧版本兼容，已废弃
    QMap<QString, QDateTime> m_lastTrayTime; ///< 每个托盘的最后处理时间（key格式: "real_1" 或 "empty_1"）
    QLabel* labelConnectionStatus;
    QString m_password;           ///< 存储的密码
    bool m_isPasswordSet;         ///< 密码是否已设置
    
    // 数据库配置
    QString m_dbHost;             ///< 数据库主机地址
    int m_dbPort;                 ///< 数据库端口
    QString m_dbName;             ///< 数据库名称
    QString m_dbUsername;         ///< 数据库用户名
    QString m_dbPassword;         ///< 数据库密码

    // 日志系统成员变量
    QString m_logDirectory;       ///< 日志目录路径
    QString m_logFileName;        ///< 当前日志文件名
    QFile* m_logFile;             ///< 日志文件对象
    QTextStream* m_logStream;     ///< 日志流对象
    QMutex m_logMutex;            ///< 日志互斥锁
    static tcpClient* s_instance; ///< 静态实例指针（用于静态日志处理器）
};

#endif // TCPCLIENT_H
