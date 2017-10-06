#include "QTelnetInterface.h"


/**
 * @brief QTelnetInterface::addCommand
 * Adds a new command to the queue.
 * @param cmd The command to send.
 * @param prompt The prompt to wait to for a succesfull command.
 * @param errors Lists of paired texts for errors and their explanation text.
 */
void QTelnetInterface::addCommand(const QString &label, const QString &cmd, const QString &prompt, const OLTConstants::ErrorStrings &errors, OLTState okState)
{
	Q_ASSERT_X( !label.isEmpty(), "addCommand", "Label is empty" );
	Q_ASSERT_X( (label == "InitialWelcome") || !cmd.isEmpty(), "addCommand", "Command is empty" );
	Q_ASSERT_X( !prompt.isEmpty(), "addCommand", "Prompt is empty" );

	CommandControl newCmd;
	newCmd.label = label;
	newCmd.cmd = cmd;
	newCmd.prompt = prompt;
	newCmd.errorStrings = errors;
	newCmd.state = okState;
	m_commandsQueue.append( newCmd );
		playQueue();
}

void QTelnetInterface::setOltState(QTelnetInterface::OLTState newState)
{
	if( m_OLTState != newState )
	{
		m_OLTState = newState;
		emit oltStateChanged( m_OLTState );
	}
}

QTelnetInterface::QTelnetInterface()
{
	connect( this, SIGNAL(newData(const char*,int)), this, SLOT(onDataFromOLT(const char*,int)) );
	connect( this, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onTelnetStateChange(QAbstractSocket::SocketState)) );
}

void QTelnetInterface::connectToOLT(const QString &host, quint16 port, const QString &uname, const QString &upass)
{
	addLoginCommand( "InitialWelcome", "", oltConstants.constantUName(), OltLogging );
	addLoginCommand( "UsernameLogin", uname, oltConstants.constantUPass(), OltLogging );
	addLoginCommand( "PasswordLogin", upass, oltConstants.promptInitial(), OltLogged );
	QTelnet::connectToHost(host, port);
}

void QTelnetInterface::playQueue()
{
	if( (state() == QAbstractSocket::ConnectedState) && m_commandsQueue.count() && m_currentCommand.isCleared() )
	{
		m_currentCommand = m_commandsQueue.takeFirst();
		m_dataBuffer.clear();
		if( !m_currentCommand.cmd.isEmpty() )
		{
			sendData( m_currentCommand.cmd.toLatin1() );
			sendData( "\n" );
		}
	}
}

void QTelnetInterface::onDataFromOLT(const char *data, int length)
{
	m_dataBuffer.append( QByteArray(data, length) );
	QString err = m_currentCommand.errorStrings.errorString(m_dataBuffer);
	if( !err.isEmpty() )
	{
		emit errorResponse(m_currentCommand.label, m_currentCommand.cmd, err);
		m_dataBuffer.clear();
		m_currentCommand.clear();
		playQueue();
	}
	else if( m_dataBuffer.contains("---- More ( Press 'Q' to break ) ----") )
	{
		m_dataBuffer.replace("---- More ( Press 'Q' to break ) ----", "");
		sendData( " " );
	}
	else if( m_dataBuffer.contains(m_currentCommand.prompt) )
	{
		if( m_currentCommand.state != OltUnknownSate )
			setOltState(m_currentCommand.state);
		emit newResponse(m_currentCommand.label, m_currentCommand.cmd, m_dataBuffer);
		m_dataBuffer.clear();
		m_currentCommand.clear();
		playQueue();
	}
}

void QTelnetInterface::onTelnetStateChange(QAbstractSocket::SocketState st)
{
	switch( st )
	{
	case QAbstractSocket::UnconnectedState:
		m_OLTState = OltUnconnected;
		break;
	case QAbstractSocket::HostLookupState:
		m_OLTState = OltUnconnected;
		break;
	case QAbstractSocket::ConnectingState:
		m_OLTState = OltUnconnected;
		break;
	case QAbstractSocket::ConnectedState:
		m_OLTState = OltConnected;
		playQueue();
		break;
	case QAbstractSocket::BoundState:
		m_OLTState = OltUnconnected;
		break;
	case QAbstractSocket::ListeningState:
		break;
		m_OLTState = OltUnconnected;
	case QAbstractSocket::ClosingState:
		m_OLTState = OltUnconnected;
		break;
	}
}
