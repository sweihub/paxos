// Paxos Demo 
// Sunding Wei, weisunding@gmail.com
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <map>
#include <list>

// How many processes you want to start
#define NODE_COUNT    3

// Consensus timeout in seconds
#define TIMEOUT 1

#define PAXOS_PREPARE 1
#define PAXOS_PROMISE 2
#define PAXOS_ACCEPT  3
#define PAXOS_OK      4
#define PAXOS_NOTIFY  5

struct Paxos {
    int type;
    int pn; // proposal number
    int value;
    int node;
};

struct Node {
    int fd;
    int id;
    int port;
    int pn;
    int state;
    int value;

    // as acceptor
    int promise;
    int accepted_pn;
    int accepted_value;

    // as proposer
    std::map<int, Paxos> consensus;
    std::map<int, Paxos> promises;

    // chonsen
    time_t tick;
    int chosen_pn;
    int chosen_value;

    std::list<Paxos> q;
};

int socket_send(int fd, Paxos *buf, sockaddr_in * to)
{
    int n = sendto(fd, (const char *)buf, sizeof(*buf),  0, 
            (const struct sockaddr *)to, sizeof(*to)); 
    return (n);
}

int send(Node * ctx, Paxos *buf, int node)
{
    if (node == ctx->id) {
        ctx->q.push_back(*buf);
        return sizeof(*buf);
    }

    struct sockaddr_in to = {0};
    to.sin_family = AF_INET; 
    to.sin_addr.s_addr = inet_addr("127.0.0.1");
    to.sin_port = htons(node + 5000);
    return socket_send(ctx->fd, buf, &to);
}

int set_timeout(int fd, int ms) 
{
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms * 1000 - tv.tv_sec*1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void prepare(Node *node)
{
    node->consensus.clear();
    node->promises.clear();

    // Just for demo purpose, the nodes = {1,2,3,4,5}
    for (int i = 1; i <= NODE_COUNT; i++) {
        Paxos paxos = {0};
        paxos.type = PAXOS_PREPARE;
        paxos.pn = node->pn;
        paxos.node = node->id;
        int n = send(node, &paxos, i);
        if (n < 0)
            perror("send");
        printf("   send prepare to node %d, [PN: %d]\n", i, paxos.pn);
    }

    // process promises
    node->state = 1;
}

int next_value(Node * node)
{
    return node->id * 1000 + std::rand() % 100;
}

void handle_promise(Node * node)
{
    if (node->promises.size() <= NODE_COUNT / 2.0)
        return;

    int maxpn = -1;

    // select a value of max PN if any
    for (auto &p:node->promises) {
        Paxos * paxos = &p.second;
        if (paxos->pn > maxpn) {
            maxpn = paxos->pn;
            node->value = paxos->value;
        }
    }

    if (node->value <= 0 ) {
        node->value = next_value(node);
        printf("   I propose [PN: %d,  value: %d]\n", node->pn, node->value);
    }

    Paxos accept = {0};
    accept.type = PAXOS_ACCEPT;
    accept.node = node->id;
    accept.pn = node->pn;
    accept.value = node->value;

    for (auto &p:node->promises) {
        Paxos * paxos = &p.second;
        printf("   send accept to node %d, [PN: %d, value: %d]\n",
            paxos->node, accept.pn, accept.value);
        send(node, &accept, paxos->node);
    }

    // move on to accept stage
    node->state = 2;
}

void respond_accept(Node * node, Paxos * paxos)
{
    if (paxos->pn < node->promise) {
        printf("   ignore accept (node: %d, [PN: %d, value: %d]), my promised PN: %d\n", 
            paxos->node, paxos->pn, paxos->value, node->promise); 
        return;
    }

    node->accepted_pn = paxos->pn;
    node->accepted_value = paxos->value;

    Paxos ok = {0};
    ok.type = PAXOS_OK;
    ok.pn = paxos->pn;
    ok.value = paxos->value;
    ok.node = node->id;
    send(node, &ok, paxos->node);
}

void notify_learners(Node * node)
{
    Paxos note = {0};

    note.type = PAXOS_NOTIFY;
    note.pn = node->pn;
    note.value = node->value;
    note.node = node->id;

    // notify any learners as you define
    for (int i = 1; i <= NODE_COUNT; i++) {
        printf("   notify node %d, proposal (node: %d, [PN: %d, value: %d])\n",
            i, note.node, note.pn, note.value);
        send(node, &note, i);
    }
}

// As proposer, we check the accept response
void handle_consensus(Node * node)
{
    int i = 0;
    for (auto &p:node->consensus) {
        Paxos& paxos = p.second;
        if (paxos.pn == node->pn && paxos.value == node->value)
            ++i;
    }

    if (i > NODE_COUNT / 2.0) {
        printf("   As proposer, chosen (node: %d, [PN: %d, value: %d])\n", 
            node->id, node->pn, node->value);
        // move on to chosen state
        node->state = 3;
        node->chosen_pn = node->pn;
        node->chosen_value = node->value;
        node->tick = time(nullptr);
        notify_learners(node);
    }
}

void heartbeat(Node * node)
{
    switch (node->state) {
        case 0: // prepre
            prepare(node);
            break;
        case 1: // handle promises, then send accept
            handle_promise(node);
            break;
        case 2: // handle accept, then check if any value chosen
            handle_consensus(node);
            break;
        case 3: // chosen
            break;
        default:
            break;
    }

    // Restart if no consensus. 
    // NOTE: In real world,
    // 1. Each node should generate globally unique PN
    // 2. If the proposer node was dead for long, the consensus should be restarted  
    // 3. Each node should remember the consensus state when restart
    if (time(nullptr) - node->tick >= TIMEOUT && node->chosen_pn == 0) {
        printf("\nRestart since no consensus\n");
        node->state = 0;
        node->pn++;
        node->tick = time(nullptr);
    }
}

// PROMISE response to PREPARE
void respond_prepare(Node * node, Paxos * paxos)
{
    if (paxos->pn < node->promise) {
        printf("   ignore prepare (node: %d, PN: %d), my promised PN: %d\n", 
            paxos->node, paxos->pn, node->promise);
        return;
    }

    // promis to accept any PN greater than paxos->pn
    node->promise = paxos->pn;

    Paxos reply = {0};
    reply.node = node->id;
    reply.type = PAXOS_PROMISE;
    reply.pn = node->accepted_pn;
    reply.value = node->accepted_value;
    printf("   send promise to node %d, [PN: %d]\n", paxos->node, paxos->pn);
    send(node, &reply, paxos->node);
}

void handle_paxos(Node * node, Paxos* paxos)
{
    switch (paxos->type) {
        case PAXOS_PREPARE:
            respond_prepare(node, paxos);
            break;
        case PAXOS_PROMISE:
            printf("   recv promise from node: %d, accepted [PN: %d]\n", paxos->node, paxos->pn);
            node->promises[paxos->node] = *paxos;
            break;
        case PAXOS_ACCEPT:
            printf("   recv accept from node %d, [PN: %d, value: %d]\n", 
                paxos->node, paxos->pn, paxos->value);
            respond_accept(node, paxos);
            break;
        case PAXOS_OK:
            printf("   recv OK from node: %d, [PN: %d, value: %d]\n", 
                paxos->node, paxos->pn, paxos->value);
            node->consensus[paxos->node] = *paxos;
            break;
       case PAXOS_NOTIFY:
            printf("   recv notify from node: %d, [PN: %d, value: %d]\n", 
                paxos->node, paxos->pn, paxos->value);
            node->chosen_pn = paxos->pn;
            node->chosen_value = paxos->value;
            node->tick = time(nullptr);
            printf("Chosen (node: %d, [PN: %d, value: %d])\n", 
                paxos->node, paxos->pn, paxos->value);
            break;
    }
}

void process(Node *node, Paxos * paxos)
{
    if (paxos != nullptr)
        handle_paxos(node, paxos);
    heartbeat(node);
}

int main(int argc, const char * argv[]) { 
    int fd; 
    struct sockaddr_in server, client; 
    Node node = {0};

    if (argc < 2) {
        printf("Please input node number!\n");
        exit(-1);
    }

    std::srand(std::time(nullptr));
    node.id = atoi(argv[1]);
    node.port = node.id + 5000;
    node.pn = node.id * 3;
    node.tick = time(nullptr);

    // Creating socket file descriptor 
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    node.fd = fd;
    set_timeout(fd, 200);
    memset(&server, 0, sizeof(server)); 
    memset(&client, 0, sizeof(client)); 

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; 
    server.sin_port = htons(node.port); 

    // Bind the socket with the server address 
    if (bind(fd, (const struct sockaddr *)&server,  sizeof(server)) < 0 ) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    int n;
    printf("Paxos Demo by Sunding Wei (PN = Proposal Number)\n");
    printf("Node %d is running at port: %d\n", node.id, node.port);

    while (true) {
        // eat loopback data
        while (node.q.size()) {
            Paxos paxos = node.q.front();
            node.q.pop_front();
            process(&node, &paxos);
        }
        Paxos paxos;
        unsigned int len = sizeof(len);
        n = recvfrom(fd, &paxos, sizeof(paxos),  0, (struct sockaddr *)&client, &len); 
        Paxos * data = nullptr;
        if (n == sizeof(Paxos))
            data = &paxos;
        process(&node, data);
    } 

    return 0; 
} 
