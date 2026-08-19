#ifndef DATAPROCESSWORKER_H
#define DATAPROCESSWORKER_H

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QStringList>

/**
 * @brief 报文串行调度：PLC / JSON 共用 FIFO，主线程一次只处理一条，保证顺序
 * JSON 在工作线程完成清洗、拆包、编码探测，再回主线程跑原有业务
 */
class DataProcessWorker : public QObject
{
    Q_OBJECT
public:
    explicit DataProcessWorker(QObject *parent = nullptr);

public slots:
    void submitPlcPacket(const QByteArray &data, bool ackAlreadySent);
    void submitJsonPacket(const QByteArray &data, bool saveToTable, bool fromEdSoftware);
    void plcApplyFinished();
    void jsonApplyFinished();

signals:
    void applyPlcPacket(const QByteArray &data, bool ackAlreadySent);
    void applyJsonObjects(const QStringList &jsonObjects, bool saveToTable, bool fromEdSoftware);

private:
    enum class JobType {
        Plc,
        Json
    };
    struct Job {
        JobType type = JobType::Plc;
        QByteArray data;
        bool ackAlreadySent = false;
        bool saveToTable = true;
        bool fromEdSoftware = false;
    };

    void tryDispatchNext();
    static QStringList prepareJsonObjects(const QByteArray &data);

    QQueue<Job> m_queue;
    bool m_processing = false;
};

#endif // DATAPROCESSWORKER_H
