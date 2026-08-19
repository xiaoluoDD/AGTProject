#include "dataprocessworker.h"

#include <QChar>

DataProcessWorker::DataProcessWorker(QObject *parent)
    : QObject(parent)
{
}

void DataProcessWorker::submitPlcPacket(const QByteArray &data, bool ackAlreadySent)
{
    Job job;
    job.type = JobType::Plc;
    job.data = data;
    job.ackAlreadySent = ackAlreadySent;
    m_queue.enqueue(job);
    tryDispatchNext();
}

void DataProcessWorker::submitJsonPacket(const QByteArray &data, bool saveToTable, bool fromEdSoftware)
{
    Job job;
    job.type = JobType::Json;
    job.data = data;
    job.saveToTable = saveToTable;
    job.fromEdSoftware = fromEdSoftware;
    m_queue.enqueue(job);
    tryDispatchNext();
}

void DataProcessWorker::plcApplyFinished()
{
    m_processing = false;
    tryDispatchNext();
}

void DataProcessWorker::jsonApplyFinished()
{
    m_processing = false;
    tryDispatchNext();
}

void DataProcessWorker::tryDispatchNext()
{
    if (m_processing || m_queue.isEmpty()) {
        return;
    }
    m_processing = true;
    const Job job = m_queue.dequeue();

    if (job.type == JobType::Plc) {
        emit applyPlcPacket(job.data, job.ackAlreadySent);
        return;
    }

    // JSON：在工作线程完成清洗/拆包，主线程只跑业务
    const QStringList objects = prepareJsonObjects(job.data);
    emit applyJsonObjects(objects, job.saveToTable, job.fromEdSoftware);
}

QStringList DataProcessWorker::prepareJsonObjects(const QByteArray &data)
{
    QByteArray cleanedData = data;

    while (!cleanedData.isEmpty()) {
        const char firstChar = cleanedData[0];
        if (firstChar == '`' || firstChar == '\'' || firstChar == '\0'
            || (firstChar < 0x20 && firstChar != '\n' && firstChar != '\r' && firstChar != '\t')) {
            cleanedData.remove(0, 1);
        } else {
            break;
        }
    }

    while (!cleanedData.isEmpty()) {
        const char lastChar = cleanedData[cleanedData.size() - 1];
        if (lastChar == '\0'
            || (lastChar < 0x20 && lastChar != '\n' && lastChar != '\r' && lastChar != '\t')) {
            cleanedData.remove(cleanedData.size() - 1, 1);
        } else {
            break;
        }
    }

    QString jsonString = QString::fromUtf8(cleanedData);
    if (jsonString.contains(QChar::ReplacementCharacter)
        || (jsonString.isEmpty() && !cleanedData.isEmpty())) {
        jsonString = QString::fromLocal8Bit(cleanedData);
        if (jsonString.contains(QChar::ReplacementCharacter)) {
            jsonString = QString::fromLatin1(cleanedData);
        }
    }

    QStringList jsonObjects;
    int startPos = 0;
    int braceCount = 0;
    bool inString = false;
    bool escapeNext = false;

    for (int i = 0; i < jsonString.length(); ++i) {
        const QChar ch = jsonString[i];

        if (escapeNext) {
            escapeNext = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escapeNext = true;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            inString = !inString;
            continue;
        }
        if (!inString) {
            if (ch == QLatin1Char('{')) {
                if (braceCount == 0) {
                    startPos = i;
                }
                ++braceCount;
            } else if (ch == QLatin1Char('}')) {
                --braceCount;
                if (braceCount == 0) {
                    jsonObjects.append(jsonString.mid(startPos, i - startPos + 1));
                }
            }
        }
    }

    if (jsonObjects.isEmpty() && !jsonString.trimmed().isEmpty()) {
        jsonObjects.append(jsonString);
    }
    return jsonObjects;
}
