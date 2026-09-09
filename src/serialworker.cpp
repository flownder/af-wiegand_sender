#include "serialworker.h"

#include <QDebug>
#include <QThread>

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
{
}

SerialWorker::~SerialWorker()
{
    closePort();
}

void SerialWorker::openPort(const QString &portName)
{
    // Если порт уже был открыт — закрываем старый
    if (m_serialPort) {
        closePort();
    }

    // ВАЖНО:
    // QSerialPort создаётся внутри слота, который будет вызван
    // уже в рабочем потоке. Это правильное поведение для QThread.
    m_serialPort = new QSerialPort(this);

    m_serialPort->setPortName(portName);

    // Скорость установлена жёстко в коде
    m_serialPort->setBaudRate(115200);

    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialWorker::onReadyRead);

    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialWorker::onSerialError);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        const QString errorText =
            QStringLiteral("Не удалось открыть порт %1: %2")
                .arg(portName, m_serialPort->errorString());

        emit errorOccurred(errorText);
        emit openedChanged(false);

        m_serialPort->deleteLater();
        m_serialPort = nullptr;
        return;
    }

    qDebug() << "Serial port opened:" << portName
             << "in thread:" << QThread::currentThread();

    emit openedChanged(true);
}

void SerialWorker::closePort()
{
    if (!m_serialPort) {
        return;
    }

    disconnect(m_serialPort, nullptr, this, nullptr);

    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->deleteLater();
    m_serialPort = nullptr;

    emit openedChanged(false);
}

void SerialWorker::sendData(const QByteArray &data)
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->write(data);
    }
}

void SerialWorker::onReadyRead()
{
    if (!m_serialPort) {
        return;
    }

    const QByteArray receivedData = m_serialPort->readAll();

    if (!receivedData.isEmpty()) {
        emit dataReceived(receivedData);
    }
}

void SerialWorker::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    if (!m_serialPort) {
        return;
    }

    const QString errorText =
        QStringLiteral("Ошибка последовательного порта %1: %2")
            .arg(m_serialPort->portName(), m_serialPort->errorString());

    emit errorOccurred(errorText);
}
