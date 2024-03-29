#include <iostream>
#include <winsock2.h>
#include <bits/stdc++.h>
#include <thread>

using namespace std;

SOCKET serverSocket;
typedef struct packet {
    uint16_t len;
    uint32_t seq_no;
    char payload[1024];
} packet;
typedef struct ack_packet {
    uint16_t len;
    uint32_t ack_no;
} ack_packet;
#define slowStart 1
#define congestionAvoidance 2
#define fastRecovery 3
default_random_engine generator(5);
uniform_real_distribution<float> distribution(0.0, 1.0);
float lossProbability = 0.1;

void handleClient(struct sockaddr_in clientAddress, string fileName, int uniquePort) {
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(uniquePort);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        cout << "Bind failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return;
    }

    ifstream file("../Server/" + fileName, ios::binary);
    const int payload_size = sizeof(packet::payload);
    int cwnd = 1; // should be initially 1
    uint32_t seq_no = 1;  // Sequence number for packets
    int clientLen = sizeof(clientAddress);
    int state = slowStart;
    int nextState = state;
    int ssthresh = 400;
    ofstream file2("../Server/analysis.txt" , ios::binary);
    int counter = 0;

    while (!file.eof()) {
        cout << "current cwnd: " << cwnd << endl;
        file2 << to_string(counter++) << '\t' << to_string(cwnd) << '\n';
        int notAckedPkts = 0;
        map<uint32_t, packet> pktMap;
        packet pkt;
        for(int i = 0; i < cwnd && !file.eof(); i++) {
            file.read(pkt.payload, payload_size);
            streamsize bytes_read = file.gcount();

            pkt.len = static_cast<uint16_t>(bytes_read + sizeof(pkt.seq_no) + sizeof(pkt.len));
            pkt.seq_no = seq_no++;

            if (distribution(generator) > lossProbability) {
                sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress, sizeof(clientAddress));
            }
            notAckedPkts++;
            pktMap[pkt.seq_no] = pkt;
        }

        uint32_t lastAckedPkt = 0;
        int duplicateAck = 0;
        int nextCwnd = INT_MAX / 2;
        int nextssthresh = ssthresh;
        while(lastAckedPkt != pkt.seq_no) {
            ack_packet ack;
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(clientSocket, &readfds);
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 400000;
            if (select(clientSocket + 1, &readfds, nullptr, nullptr, &tv) == 0) {
                cout << "timeout occurred" << endl;
                packet pkt = pktMap[lastAckedPkt + 1];
                sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress,sizeof(clientAddress));
                nextState = slowStart;
                nextssthresh = max(cwnd / 2, 1);
                nextCwnd = 1;
                duplicateAck = 0;
            }
            else {
                recvfrom(clientSocket, (char *) &ack, sizeof(ack), 0, (struct sockaddr *) &clientAddress, &clientLen);
                cout << "ack: " << ack.ack_no << endl;
                if (lastAckedPkt < ack.ack_no) { // new ack
                    duplicateAck = 0;
                    lastAckedPkt = ack.ack_no;
                    switch (state) {
                        case slowStart:
                            if (cwnd * 2 >= ssthresh && cwnd + 1 < nextCwnd) {
                                nextState = congestionAvoidance;
                                nextCwnd = cwnd + 1;
                            }
                            else if(cwnd * 2 < nextCwnd) {
                                nextState = slowStart;
                                nextCwnd = cwnd * 2;
                            }
                            break;
                        case congestionAvoidance:
                            if(cwnd + 1 < nextCwnd) {
                                nextCwnd = cwnd + 1;
                                nextState = congestionAvoidance;
                            }
                            break;
                        case fastRecovery:
                            if (ssthresh < nextCwnd) {
                                nextCwnd = ssthresh;
                                nextState = congestionAvoidance;
                            }
                            break;
                    }
                } else if(lastAckedPkt == ack.ack_no) {  // lastAckedPkt == ack.ack_no // duplicate ack
                    cout << "duplicate ack received" << endl;
                    duplicateAck++;
                    if (state == fastRecovery) {
                        packet pkt = pktMap[ack.ack_no + 1];
                        sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress,sizeof(clientAddress));
                    } else if (duplicateAck == 3) {
                        cout << "duplicate ack number = 3" << endl;
                        packet pkt = pktMap[ack.ack_no + 1];
                        sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress,sizeof(clientAddress));
                        if (ssthresh < nextCwnd) {
                            nextState = fastRecovery;
                            nextssthresh = max(cwnd / 2, 1);
                            nextCwnd = ssthresh;
                        }
                    }
                }
            }
        }
        ssthresh = nextssthresh;
        cwnd = nextCwnd;
        state = nextState;
    }
    packet endOfTransmissionPkt;
    endOfTransmissionPkt.len = 0;
    endOfTransmissionPkt.seq_no = UINT32_MAX;
    sendto(clientSocket, (char *) &endOfTransmissionPkt, sizeof(endOfTransmissionPkt), 0, (sockaddr *) &clientAddress, sizeof(clientAddress));
    closesocket(clientSocket);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Failed to initialize Winsock" << endl;
        exit(-1);
    }

    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket == -1) {
        cout << "Failed to create server socket" << endl;
        exit(-1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(80);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&serverAddress.sin_zero, 0, 8);

    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) != 0) {
        cout << "Failed to bind server socket to server address" <<endl;
        exit(-1);
    }

    char buffer[1024];
    struct sockaddr_in clientAddress;
    int clientLen = sizeof(clientAddress);

    int uniquePort = 40000;

    while(true) {
        // A new client connected and requested a file
        int bytesRead = recvfrom(serverSocket, buffer, 1024, 0, (struct sockaddr *) &clientAddress, &clientLen);
        buffer[bytesRead] = '\0';
        cout << "new client connected with message " << buffer << endl;

        // Assign to the client a unique port number
        string message = to_string(++uniquePort);
        sendto(serverSocket, message.c_str(), message.length(), 0, (sockaddr *) &clientAddress, sizeof(clientAddress));
        thread(handleClient, clientAddress, buffer, uniquePort).detach();
    }
}





//void handleClient(struct sockaddr_in clientAddress, string fileName, int uniquePort) {
//    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
//    struct sockaddr_in serverAddress;
//    memset(&serverAddress, 0, sizeof(serverAddress));
//    serverAddress.sin_family = AF_INET;
//    serverAddress.sin_port = htons(uniquePort);
//    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
//
//    if (bind(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
//        cout << "Bind failed with error: " << WSAGetLastError() << endl;
//        closesocket(clientSocket);
//        return;
//    }
//
//    ifstream file("../Server/" + fileName, ios::binary);
//    const int payload_size = sizeof(packet::payload);
//    const int window_size = 4;  // Example window size
//    vector<packet> window;
//    vector<bool> ack_received(window_size, false);
//
//    uint32_t seq_no = 0;  // Sequence number for packets
//    int clientLen = sizeof(clientAddress);
//
//    while (!file.eof() || any_of(ack_received.begin(), ack_received.end(), [](bool ack){ return !ack; })) {
//        // Fill the window with new packets
//        while (window.size() < window_size && !file.eof()) {
//            packet pkt;
//            file.read(pkt.payload, payload_size);
//            streamsize bytes_read = file.gcount();
//
//            pkt.len = static_cast<uint16_t>(bytes_read + sizeof(pkt.seq_no) + sizeof(pkt.len));
//            pkt.seq_no = seq_no++;
//
//            if (distribution(generator) > lossProbability) {
//                sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress, sizeof(clientAddress));
//            }
//
//            window.push_back(pkt);
//            ack_received.push_back(false);
//        }
//
//        // Check for acknowledgments
//        fd_set readset;
//        FD_ZERO(&readset);
//        FD_SET(clientSocket, &readset);
//        timeval timeout;
//        timeout.tv_sec = 2;  // Shorter timeout
//        timeout.tv_usec = 0;
//
//        if (select(clientSocket + 1, &readset, nullptr, nullptr, &timeout) > 0) {
//            ack_packet ack;
//            recvfrom(clientSocket, (char *) &ack, sizeof(ack), 0, (struct sockaddr*)&clientAddress, &clientLen);
//            for (int i = 0; i < window.size(); ++i) {
//                if (window[i].seq_no == ack.ack_no) {
//                    cout << "ack: " << ack.ack_no << endl;
//                    ack_received[i] = true;
//                    break;
//                }
//            }
//        } else {
//            // Timeout: retransmit all unacknowledged packets
//            for (int i = 0; i < window.size(); ++i) {
//                if (!ack_received[i]) {
//                    sendto(clientSocket, (char*)&window[i], window[i].len + 2, 0, (sockaddr*)&clientAddress, sizeof(clientAddress));
//                }
//            }
//        }
//
//        // Slide window forward
//        while (!window.empty() && ack_received.front()) {
//            window.erase(window.begin());
//            ack_received.erase(ack_received.begin());
//        }
//    }
//
//    closesocket(clientSocket);
//}






/*void handleClient(struct sockaddr_in clientAddress, string fileName, int uniquePort) {
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(uniquePort);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        cout << "Bind failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return;
    }

    ifstream file("../Server/" + fileName, ios::binary);
    packet pkt;
    ack_packet ack;
    uint32_t seq_no = 0;
    const int payload_size = sizeof(pkt.payload);
    int clientLen = sizeof(clientAddress);

    while (!file.eof()) {
        file.read(pkt.payload, payload_size);
        streamsize bytes_read = file.gcount();

        pkt.len = static_cast<uint16_t>(bytes_read + sizeof(pkt.seq_no) + sizeof(pkt.len));
        pkt.seq_no = seq_no;

        RESEND:
        if(distribution(generator) > lossProbability) {
            sendto(clientSocket, (char *) &pkt, pkt.len + 2, 0, (sockaddr *) &clientAddress, sizeof(clientAddress));
        }

        // Wait for acknowledgment
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(clientSocket, &readset);
        timeval timeout;
        timeout.tv_sec = 5;  // Set timeout (e.g., 5 seconds)
        timeout.tv_usec = 0;

        if (select(clientSocket + 1, &readset, nullptr, nullptr, &timeout) > 0) {
            bytes_read = recvfrom(clientSocket, (char *) &ack, sizeof(ack), 0, (struct sockaddr*)&clientAddress, &clientLen);
            if (ack.ack_no == seq_no) {
                cout << "ack: " << ack.ack_no <<endl;
                // Ack received, proceed
                seq_no++;
                // Adjust congestion window here
            } else {
                // Handle incorrect ack
            }
        } else {
            cout << "timeout" << endl;
            goto RESEND;
            // Timeout, handle retransmission
        }
    }
    closesocket(clientSocket);
*/