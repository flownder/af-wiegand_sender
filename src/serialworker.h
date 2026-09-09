#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QByteArray>
#include <QSerialPort>

class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

public slots:
    void openPort(const QString &portName);
    void closePort();
    void sendData(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &errorText);
    void openedChanged(bool opened);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serialPort = nullptr;
};

#endif // SERIALWORKER_H
