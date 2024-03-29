#include <iostream>
#include <winsock2.h>
#include <bits/stdc++.h>
#include <unistd.h>
using namespace std;

typedef struct packet {
    uint16_t len;
    uint32_t seq_no;
    char payload[1024];
} packet;
typedef struct ack_packet {
    uint16_t len;
    uint32_t ack_no;
} ack_packet;

int main() {

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Failed to initialize Winsock" << endl;
        exit(-1);
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket == -1) {
        cout << "Failed to create client socket" << endl;
        exit(-1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(80);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    int serverLen = sizeof(serverAddress);

    string message;
    cin >> message;
    sendto(clientSocket, message.c_str(), message.length(), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    char buffer[1024];
    int bytesReceived = recvfrom(clientSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&serverAddress, &serverLen);
    buffer[bytesReceived] = '\0';
    int uniquePort = atoi(buffer);
    serverAddress.sin_port = htons(uniquePort);

    ofstream file("../Client/" + message, ios::binary);
    packet pkt;
    ack_packet ack;
    uint32_t expected_seq_no = 1;

    map<uint32_t, packet> pktMap;

    while (true) {
        int recv_len = recvfrom(clientSocket, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr *)&serverAddress, &serverLen);

        cout << "Received packet with sequence number: " << pkt.seq_no << endl;
        if (pkt.seq_no == UINT32_MAX) {
            while (!pktMap.empty()) {
                packet& nextPkt = pktMap.begin()->second;
                cout << "write pkt:" << nextPkt.seq_no << endl;
                file.write(nextPkt.payload, nextPkt.len - sizeof(nextPkt.seq_no) - sizeof(nextPkt.len));
                pktMap.erase(pktMap.begin());
            }
            break;
        }

        ack.len = sizeof(ack_packet);
        if(pkt.seq_no > expected_seq_no) {
            ack.ack_no = expected_seq_no - 1;
            pktMap[pkt.seq_no] = pkt;
        }
        else if(pkt.seq_no == expected_seq_no) { // pkt.seq_no == expected_seq_no
            cout << "write pkt:" << pkt.seq_no << endl;
            file.write(pkt.payload, pkt.len - sizeof(pkt.seq_no) - sizeof(pkt.len));
            ack.ack_no = expected_seq_no;
            expected_seq_no++; // Increment the expected sequence number for the next packet

            while (!pktMap.empty() && pktMap.begin()->first == expected_seq_no) {
                packet& nextPkt = pktMap.begin()->second;
                cout << "write pkt:" << nextPkt.seq_no << endl;
                file.write(nextPkt.payload, nextPkt.len - sizeof(nextPkt.seq_no) - sizeof(nextPkt.len));
                pktMap.erase(pktMap.begin());
                ack.ack_no = expected_seq_no;
                expected_seq_no++;
            }
        }
        sendto(clientSocket, (char*)&ack, sizeof(ack), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    }

}
