#include <unistd.h>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QUdpSocket>
#include <QtCore/QDateTime>
#include <string> 
#include <QHostInfo>

#include "main.hh"

const int ChatDialog::RUMOR_TIMEOUT = 1000;
const int ChatDialog::ANTI_ENTROPY_TIMEOUT = 5000;

ChatDialog::ChatDialog()
{
	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	sock = new NetSocket();
	sock->bind();
	setWindowTitle("P2Papp " + sock->id);

	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));

	connect(sock, SIGNAL(readyRead()), 
		this, SLOT(rev_data()));

	rumor_timer = new QTimer(this);

	connect(rumor_timer, SIGNAL(timeout()), 
		this, SLOT(rumor_timeout()));

	entropy_timer = new QTimer(this);

	connect(entropy_timer, SIGNAL(timeout()), 
		this, SLOT(entropy_timeout()));

	entropy_timer->start(ANTI_ENTROPY_TIMEOUT);
}

void ChatDialog::rumor_timeout() 
{
    qDebug() << "Rumor Timeout";
    return;
}

void ChatDialog::entropy_timeout() 
{
    qDebug() << "Anti Entropy Timeout";

    entropy_timer->start(ANTI_ENTROPY_TIMEOUT);
    quint16 next_neighbor = sock->pickNeighbor();
    sock->send_status_ack(next_neighbor);

}

void ChatDialog::create_new_node(QString id){
	sock->messageMap.insert(id, QVector<QString>());
	sock->serMap.insert(id, 0);
}

void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	// Insert some networking code here...
	qDebug() << "FIX: send message to other peers: " << textline->text();
	QString curr_msg = sock->id + ": " + textline->text();
	textview->append(curr_msg);
	if(!sock->serMap.contains(sock->id) ){
		create_new_node(sock->id);
	}
	sock->messageMap[sock->id].append(curr_msg);
	quint32 ser = sock->serMap[sock->id].toUInt();
	sock->serMap[sock->id] = ser + 1;

	quint16 next_neighbor = sock->pickNeighbor();
	rumor_timer->start(RUMOR_TIMEOUT);
	sock->rumor(sock->id, next_neighbor, ser, sock->messageMap[sock->id][ser]);

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::rev_data(){
    	if (sock->pendingDatagramSize() == -1) {
        	qDebug() << "No Datagram\n";
        	exit(-1);
	}

	QByteArray datagram;
	// copy from http://doc.qt.io/qt-4.8/qudpsocket.html
	if (sock->hasPendingDatagrams()) {
		qDebug() << "Has Pending Datagrams\n";
        	datagram.resize(sock->pendingDatagramSize());
        	QHostAddress sender;
        	quint16 senderPort;

        	sock->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        	QDataStream dataStream(&datagram, QIODevice::ReadOnly);
		QVariantMap allMessageMap;
		dataStream >> allMessageMap;

		//status Message 
		if(allMessageMap.contains("Want")){
			QMap<QString, QVariant> nestMap = allMessageMap["Want"].toMap();
			status_message_handler(nestMap, senderPort);
		}else{
			rumer_message_handler(allMessageMap);
			sock->send_status_ack(senderPort);
		}
    	}
}

void ChatDialog::rumer_message_handler(QMap<QString, QVariant> allMessage)
{
	entropy_timer->stop();
	entropy_timer->start(ANTI_ENTROPY_TIMEOUT);
	QString chat_str = allMessage["ChatText"].toString();
	QString origin = allMessage["Origin"].toString();
	quint32 seqNum = allMessage["seqNum"].toUInt();
	if(!sock->serMap.contains(origin))
	{
		create_new_node(origin);

	}
	quint32 my_seqNum = sock->serMap[origin].toUInt();
	if(my_seqNum == seqNum){
		qDebug() << "add new view\n";
		sock->serMap[origin] = seqNum + 1;
		textview->append(chat_str);
		sock->messageMap[origin].append(chat_str);
		quint16 next_neighbor = sock->pickNeighbor();
		sock->rumor(origin, next_neighbor, seqNum, sock->messageMap[origin][seqNum]);
		rumor_timer->start(RUMOR_TIMEOUT);
	}	
}

void ChatDialog::status_message_handler(QMap<QString, QVariant> nestMap, quint16 senderPort)
{
	rumor_timer->stop();
	for(QVariantMap::const_iterator it = sock->serMap.begin(); it != sock->serMap.end(); it ++){
		QString origin_id = it.key();
		quint32 seqNum = it.value().toUInt();
		
		bool not_contains = !nestMap.contains(origin_id);
		bool less_than_seqNum = nestMap[origin_id].toUInt() < seqNum;
		if(not_contains || less_than_seqNum){
			sock->last_origin = origin_id;
			sock->last_series = not_contains? 0 : nestMap[origin_id].toUInt();
			rumor_timer->start(RUMOR_TIMEOUT);
			sock->rumor(origin_id, senderPort, sock->last_series, 
					sock->messageMap[origin_id][sock->last_series]);
			return;
		}

	}

	for(QVariantMap::const_iterator it = nestMap.begin(); it != nestMap.end(); it ++){
		QString origin_id = it.key();
		quint32 seqNum = it.value().toUInt();
		bool not_contains = !nestMap.contains(origin_id);
		bool less_than_seqNum = nestMap[origin_id].toUInt() < seqNum;
		if(not_contains || less_than_seqNum){
			sock->send_status_ack(senderPort);
			return;
		}
	}

	if(!sock->isRumor)	return;

	QDateTime cd = QDateTime::currentDateTime();
	qsrand(cd.toTime_t());
	quint32 r = qrand();
	if(r & 1){
		quint16 sendPort = sock->myPort + 1;
		if(senderPort == sendPort){
			sendPort = sock->myPort - 1;
		}
		QString message = sock->messageMap[sock->last_origin][sock->last_series];
		sock->rumor(sock->last_origin, sendPort, sock->last_series, message);
		
	}else{
		sock->isRumor = false;
		return;
	}
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.

	quint32 r = getuid();
	myPortMin = 32768 + (r % 4096)*4;
	myPortMax = myPortMin + 10007;
	isRumor = false;
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
		        myPort = p;
			qDebug() << "\nbound to UDP port " << p;
			id = "< ID: " + QString::number(myPort) + "@" +  QHostInfo::localHostName() + " >";
			return true;
		}
	}
	
	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

void NetSocket::send_status_ack(quint16 senderPort){
	QVariantMap qvMap;
	QByteArray q_byte;
	qvMap.insert("Want", serMap);

	QDataStream q_data_stream(&q_byte, QIODevice::WriteOnly);
	q_data_stream << qvMap;
	writeDatagram(q_byte, QHostAddress:: LocalHost, senderPort);
}

void NetSocket::rumor(QString origin, quint16 port, quint32 series, QString msg){
	qDebug()<< "rumor port number is " << port;
	isRumor = true;
	QVariantMap qvMap;
	qvMap.insert("ChatText", msg);
	qvMap.insert("Origin", origin);
	qvMap.insert("seqNum",series);
	QByteArray q_byte;
	QDataStream q_data_stream(&q_byte, QIODevice::WriteOnly);
	q_data_stream << qvMap;
	last_origin = origin;
	last_series = series;
	writeDatagram(q_byte, QHostAddress:: LocalHost, port);
}

quint16 NetSocket::pickNeighbor(){
	qDebug()<< "\nPick Neighbor\n" << "myPortMin is " << myPortMin << "\nmyPortMax is " << myPortMax;
	if(myPortMin == myPort){
		return myPort + 1;
	}else if (myPortMax == myPort){
		return myPort - 1;
	}else{
		int r = qrand();
		return (r & 1) ? myPort - 1 : myPort + 1; 
	}
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
		
	//NetSocket sock;
	/*if (!sock.bind())
		exit(1);*/

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

