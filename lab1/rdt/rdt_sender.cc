/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

/**
 * 数据包的结构：
 * |<-  1 byte  ->|<-   4 bytes   ->|<-  4 bytes ->|<-  the rest  ->|
 * | payload size | sequence number |   checksum   |<-  payload   ->|
 *
 * payplad size: 表示 payload 的大小
 * sequence number: 表示数据包的序列号
 * checksum: 表示数据包的校验和
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <vector>
#include <functional>

#include "rdt_struct.h"
#include "rdt_sender.h"

// ------------------------- 常量定义 -------------------------
const int WINDOW_SIZE = 10;                   // 窗口大小
const int MAX_PAYLOAD_SIZE = RDT_PKTSIZE - 9; // 最大 payload 大小

// ------------------------- 全局变量 -------------------------
std::vector<packet> packet_buffer; // 数据包缓冲区
int sequence_number = 0;           // 数据包的序列号
std::mutex send_mutex;             // 互斥锁

// ------------------------- 函数声明 -------------------------
std::hash<std::string> hash_fn; // hash 函数

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    send_mutex.lock();

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > MAX_PAYLOAD_SIZE)
    {
        /* calculate checksum */
        std::string payload(msg->data + cursor, MAX_PAYLOAD_SIZE);
        int checksum = hash_fn(std::to_string(MAX_PAYLOAD_SIZE) + std::to_string(sequence_number) + payload);
        /* fill in the packet */
        packet pkt;
        pkt.data[0] = MAX_PAYLOAD_SIZE;
        memcpy(pkt.data + 1, &sequence_number, 4);
        memcpy(pkt.data + 5, &checksum, 4);
        memcpy(pkt.data + 9, msg->data + cursor, MAX_PAYLOAD_SIZE);

        /* send it out through the lower layer */
        Sender_ToLowerLayer(&pkt);
        fprintf(stdout, "At %.2fs: sender sending packet %d ...\n", GetSimulationTime(), sequence_number);

        /* save the packet in the buffer */
        packet_buffer.push_back(pkt);

        /* move the cursor */
        cursor += MAX_PAYLOAD_SIZE;

        /* update the sequence number */
        sequence_number++;
    }

    /* send out the last packet */
    if (msg->size > cursor)
    {
        /* calculate checksum */
        std::string payload(msg->data + cursor, msg->size - cursor);
        int checksum = hash_fn(std::to_string(msg->size - cursor) + std::to_string(sequence_number) + payload);
        /* fill in the packet */
        packet pkt;
        pkt.data[0] = msg->size - cursor;
        memcpy(pkt.data + 1, &sequence_number, 4);
        memcpy(pkt.data + 5, &checksum, 4);
        memcpy(pkt.data + 9, msg->data + cursor, pkt.data[0]);

        /* send it out through the lower layer */
        Sender_ToLowerLayer(&pkt);
        fprintf(stdout, "At %.2fs: sender sending packet %d ...\n", GetSimulationTime(), sequence_number);

        /* save the packet in the buffer */
        packet_buffer.push_back(pkt);

        /* update the sequence number */
        sequence_number++;
    }

    send_mutex.unlock();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
}
