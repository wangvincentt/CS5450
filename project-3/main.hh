#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>
class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	bool isRumor; 
	int myPortMin;
	int myPortMax;
	int myPort;
	quint32 last_series;
		        
        QString id;
	QString last_origin;
        
        QMap<QString, QVariant> serMap;
        QMap<QString, QVector<QString> > messageMap;

	NetSocket();
	void rumor(QString origin, quint16 port, quint32 series, QString msg);
	void send_status_ack(quint16 senderPort);
	quint16 pickNeighbor();

public slots:
	// Bind this socket to a P2Papp-specific default port
	bool bind();
};

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
	void rumer_message_handler(QMap<QString, QVariant> allMessageHandler);
	void status_message_handler(QMap<QString, QVariant> nestMap, quint16 senderPort);

public slots:
	void gotReturnPressed();
        void create_new_node(QString id); 
	void rumor_timeout();
	void entropy_timeout();
	void rev_data();
private:
	NetSocket *sock;
        QTextEdit *textview;
        QLineEdit *textline;
	QTimer *rumor_timer;
	QTimer *entropy_timer;	
	static const int RUMOR_TIMEOUT;
	static const int ANTI_ENTROPY_TIMEOUT;
};



#endif // P2PAPP_MAIN_HH
