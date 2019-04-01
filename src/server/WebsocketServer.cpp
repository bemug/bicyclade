#include "websocket.hpp"

#include "../server/Server.hpp"

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using bicyclade::Action;
using namespace std;

WebsocketServer::websocketserver() {
}

void WebsocketServer::init(condition_variable* actionCondVar, std::queue<Action>* actionsQueue, mutex* actionLock, Server* mainServer) {

	// Initialize Asio Transport
	socketServer.init_asio();

	// Register handler callbacks
	socketServer.set_open_handler(bind(&websocketserver::on_open,this,::_1));
	socketServer.set_close_handler(bind(&websocketserver::on_close,this,::_1));
	socketServer.set_message_handler(bind(&websocketserver::on_message,this,::_1,::_2));

	this->actionCondVar = actionCondVar;
	this->actionsQueue = actionsQueue;
	this->actionLock = actionLock;
	this->mainServer = mainServer;
}


void WebsocketServer::run(uint16_t port) {
	// listen on specified port
	socketServer.listen(port);

	// Start the server accept loop
	socketServer.start_accept();

	// Start the ASIO io_service run loop
	try {
		socketServer.run();
	} catch (const std::exception & e) {
		std::cout << e.what() << std::endl;
	}
}

void WebsocketServer::on_open(connection_hdl hdl) {
	{
	    lock_guard<mutex> guard(*actionLock);
	    int id = mainServer->create_client();
	    connectionMap.insert(std::make_pair(hdl,id));
	}
}

void WebsocketServer::on_close(connection_hdl hdl) {
	{
		lock_guard<mutex> guard(*actionLock);
		//std::cout << "on_close" << std::endl;
		connectionMapType::iterator it = connectionMap.find(hdl);
		mainServer->remove_client(it->second);
		connectionMap.erase(hdl);
	}
}

void WebsocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
	// queue message up for sending by processing thread
	{
		lock_guard<mutex> guard(*actionLock);
		Action act;
        act.ParseFromString(msg->get_payload());
		actionsQueue->push(act);
	}
	actionCondVar->notify_one();
}

void WebsocketServer::broadcast(Action& action) {
    connectionMapType::iterator it;
	string sria;
	action.SerializeToString(&sria);
	for (it = connectionMap.begin(); it != connectionMap.end(); ++it) {
		socketServer.send(it->first,sria,websocketpp::frame::opcode::binary);
	}
}
