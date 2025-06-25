#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>

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

private slots:
    // 界面切换槽函数
    void onConnectionPageClicked(); ///< 切换到连接界面
    void onTablePageClicked();      ///< 切换到表格界面
    
    // 连接管理槽函数
    void onConnectClicked();      ///< 连接按钮点击处理
    void onDisconnectClicked();   ///< 断开按钮点击处理
    void onSendClicked();         ///< 发送按钮点击处理
    void onClearClicked();        ///< 清空日志按钮点击处理
    
    // 表格操作槽函数
    void onClearTableClicked();   ///< 清空表格按钮点击处理
    void onExportTableClicked();  ///< 导出表格按钮点击处理
    
    // 设置相关槽函数
    void onSetFullTrayCountClicked(); ///< 设置满托盘数量按钮点击处理
    
    // 网络事件槽函数
    void onSocketConnected();     ///< Socket连接成功处理
    void onSocketDisconnected();  ///< Socket断开连接处理
    void onSocketError(QAbstractSocket::SocketError error); ///< Socket错误处理
    void onSocketReadyRead();     ///< Socket数据可读处理
    void onConnectionTimeout();   ///< 连接超时处理

private:
    // UI和状态管理
    void setupUI();               ///< 初始化用户界面
    void setupTable();            ///< 初始化表格
    void updateConnectionStatus(bool connected); ///< 更新连接状态显示
    void appendToLog(const QString &message, bool isError = false); ///< 添加日志信息
    
    // 数据处理
    void sendData(const QByteArray &data); ///< 发送数据到服务器
    void processHexData(const QByteArray &data); ///< 处理16进制数据

private:
    Ui::tcpClient *ui;            ///< UI界面指针
    QTcpSocket *m_socket;         ///< TCP Socket对象
    QTimer *m_connectionTimer;    ///< 连接超时定时器
    bool m_isConnected;           ///< 连接状态标志
    int m_fullTrayCount;          ///< 满托盘时数量
};

#endif // TCPCLIENT_H
