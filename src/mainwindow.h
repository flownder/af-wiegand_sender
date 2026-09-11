#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QLabel>

#include "mqtt_client.h"

class QThread;
class SerialWorker;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMessageHandle(const QString &topic, const QString &message);

    void onParameterChanged();
    void onComboBoxChanged(const QString &text);
    void onCheckBoxChanged(bool checked);

    // Существующие слоты
    void on_pushButton_clicked();
    void on_sWgFrameParity_currentTextChanged(const QString &arg1);
    void on_sWgFrameLen_currentTextChanged(const QString &arg1);
    void on_pushRelay_1_clicked();
    void on_pushRelay_2_clicked();
    void on_sCASModeVariant_activated(int index);
    void on_sResultType_activated(int index);
    void on_pushButton_2_clicked();

    // Слот кнопки подключения
    void on_Connect_pushButton_clicked();

    // Новые слоты MQTT
    void onMqttConnectionStatus(bool connected);
    void onMqttConnectionError(const QString &message);

    void on_pushGPIO_1_clicked();

    void on_pushGPIO_2_clicked();

private:
    Ui::MainWindow *ui;
    mqtt_client mq;

    QLabel *mqttStatusLabel = nullptr;
    QTimer *mqttConnectTimer = nullptr;

    bool m_isConnected = false;
    bool m_connectRequested = false;
    bool m_warningShown = false;

    void setupParameterWidgets();
    void publishParameter(const QString &paramName, const QString &value);
    QString extractParamName(const QString &topic);
    void updateComboBoxByValue(QComboBox* comboBox, const QString &value, const QString &paramName);

    void updateMqttStatus(bool connected, const QString &stateText = QString());
    void showConnectionWarning(const QString &details = QString());


    // Serial port
    SerialWorker *serialWorker1 = nullptr;
    SerialWorker *serialWorker2 = nullptr;

    QThread *serialThread1 = nullptr;
    QThread *serialThread2 = nullptr;

    void setupSerial();
    void fillPortsCombo(QComboBox *combo);
    void stopSerialThread(QThread *thread, SerialWorker *worker);


    static const QString TOPIC_PREFIX_PARAMS_OUT;   // Входящие параметры (Out с точки зрения устройства = In для нас)
    static const QString TOPIC_PREFIX_STATUS_OUT;   // Статусы устройства
    static const QString TOPIC_PREFIX_CMDS_IN;      // Команды, которые мы отправляем
    static const QString TOPIC_PREFIX_PARAMS_IN;    // Параметры, которые мы отправляем

    void handleParameterMessage(const QString &paramName, const QString &message);
    void handleStatusMessage(const QString &statusName, const QString &message);
    void handleCommandMessage(const QString &commandName, const QString &message);

    QString extractSuffix(const QString &topic, const QString &prefix);




};

#endif // MAINWINDOW_H
