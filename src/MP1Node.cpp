/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include <sstream>

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);
    addToMembershipList(memberNode->addr, memberNode->heartbeat);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == strcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	MessageHdr *msg = (MessageHdr *) data;
	char *realData = (char *)(msg + 1); // don't need the MessageHdr struct which just contains the msgType

	switch (msg->msgType) {
		case JOINREQ:
			handleJoinReq(env, realData, size);
			break;
		default:
			break;
	}

	return true;
}

/**
 * This handles the join request by sending a JOINREP back to the sender.
 * Because initially all the join requests are sent to the introducer (node 1.0.0.0:0),
 * this acknowledgment is usually done by the introducer.
 *
 * incoming data is of the format [address, heartbeat]
 * the response data sent back to the sender would be a serialized version of the members in this node's membership table
 */
void MP1Node::handleJoinReq(void *env, char *data, int size) {
#ifdef DEBUGLOG
    static char s[1024];
#endif
    Address destination = getAddress(data); // the destination of this emulated send is actually the source (the sender)
	MessageHdr *response;

	string serializedNode = serialize((Member *)env);
	char *responseData = stringToCharArray(serializedNode);

    size_t responseSize = sizeof(MessageHdr) + sizeof(responseData);

	response = (MessageHdr *) malloc(responseSize * sizeof(char));
    response->msgType = JOINREP;
    memcpy((char *)(response + 1), responseData, sizeof(responseData));

#ifdef DEBUGLOG
    sprintf(s, "Acknowledged sender %s's request. Sending response back to the sender..", destination.getAddress().c_str());
	log->LOG(&memberNode->addr, s);
#endif

	// send JOINREP back to the sender
	emulNet->ENsend(&memberNode->addr, &destination, (char *)response, responseSize);
	free(response);
	free(responseData);

	// if it hasn't joined yet, add it to the membership list
	vector<MemberListEntry>::iterator it;
	it = findInList(destination);
	if (it == memberNode->memberList.end()) {
		long heartbeat;
		memcpy(&heartbeat, data + 1 + getAddressSize(), sizeof(long));
		addToMembershipList(destination, heartbeat);
	}
}

vector<MemberListEntry>::iterator MP1Node::findInList(Address address) {
	int id = getid(address);
	vector<MemberListEntry>::iterator it;
	for (it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
		if ( (*(it)).getid() == id) {
			cout << "found!" << endl;
			return it;
		}
	}
	return it;
}

/**
 * Add the node to the membership list
 */
void MP1Node::addToMembershipList(Address address, long heartbeat) {
	// value of first int af addr is the id
	int id = *(int *)(&address.addr);
	short port = *(short *)(&address.addr[4]);

	MemberListEntry mle(id, port, heartbeat, par->getcurrtime());
	memberNode->memberList.emplace_back(mle);
	log->logNodeAdd(&memberNode->addr, &address);
}

int MP1Node::getid(Address address) {
	return *(int *)(&address.addr);
}

/**
 * Serialize this node in string form
 */
string MP1Node::serialize(Member *node) {
	return "serializing!";
}

/**
 * Get size of the address
 */
size_t MP1Node::getAddressSize() {
	Address address;
	return sizeof(address.addr);
}

char* MP1Node::stringToCharArray(string s) {
	char *a = new char[s.size() + 1];
	a[s.size()] = 0;
	memcpy(a, s.c_str(), s.size());
	return a;
}

/**
 * Get address from the data. Assume first 6 bytes of the data are the address
 */
Address MP1Node::getAddress(char *data) {
	Address address;
	memcpy(address.addr, data, sizeof(address.addr));
	return address;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

char* MP1Node::getAddressStr(Address *addr) {
	char *res = NULL;
	asprintf(&res, "%d.%d.%d.%d:%d", addr->addr[0],addr->addr[1],addr->addr[2],
			addr->addr[3], *(short*)&addr->addr[4]);
	return res;
}
