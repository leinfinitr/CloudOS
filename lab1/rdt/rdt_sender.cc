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
#define WINDOW_SIZE 10       // 窗口大小
#define MAX_PAYLOAD_SIZE 119 // 最大 payload 大小 (128 - 1 - 4 - 4)
#define TIME_OUT_VALUE 0.3   // 定时器

// ------------------------- 全局变量 -------------------------
std::vector<packet> packet_buffer; // 数据包缓冲区
int sequence_number = 0;           // 数据包的序列号
std::mutex send_mutex;             // 互斥锁
int packet_in_window = 0;          // 窗口内的数据包数量

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
    // fprintf(stdout, "At %.2fs: sender lock in Sender_FromUpperLayer\n", GetSimulationTime());

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size - cursor > 0)
    {
        /* calculate payload size*/
        int payload_size = std::min(MAX_PAYLOAD_SIZE, msg->size - cursor);
        /* calculate checksum */
        std::string payload(msg->data + cursor, payload_size);
        int checksum = hash_fn(std::to_string(payload_size) + std::to_string(sequence_number) + payload);
        /* fill in the packet */
        packet pkt;
        pkt.data[0] = payload_size;
        memcpy(pkt.data + 1, &sequence_number, 4);
        memcpy(pkt.data + 5, &checksum, 4);
        memcpy(pkt.data + 9, msg->data + cursor, payload_size);

        /* send it out through the lower layer */
        if (packet_in_window < WINDOW_SIZE)
        {
            Sender_ToLowerLayer(&pkt);
            Sender_StartTimer(TIME_OUT_VALUE);
            packet_in_window++;
            // fprintf(stdout, "At %.2fs: sender sending packet %d ...\n", GetSimulationTime(), sequence_number);
        }

        /* save the packet in the buffer */
        packet_buffer.push_back(pkt);

        /* move the cursor */
        cursor += payload_size;

        /* update the sequence number */
        sequence_number++;
    }

    send_mutex.unlock();
    // fprintf(stdout, "At %.2fs: sender unlock in Sender_FromUpperLayer\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    send_mutex.lock();
    // fprintf(stdout, "At %.2fs: sender lock in Sender_FromLowerLayer\n", GetSimulationTime());

    /* get ack number, base + WINDOW_SIZE > ack number >= base */
    int ack_number = *(int *)(pkt->data + 1);
    // fprintf(stdout, "At %.2fs: sender receiving ack %d ...\n", GetSimulationTime(), ack_number);

    /* update the packet buffer */
    for (auto it = packet_buffer.begin(); it != packet_buffer.end() && it != packet_buffer.begin() + WINDOW_SIZE; it++)
    {
        int sequence_number = *(int *)(it->data + 1);
        if (sequence_number == ack_number)
        {
            /* remove the packet from the buffer */
            it = packet_buffer.erase(it);

            /* if there are packets in the buffer that can be sent */
            if (packet_buffer.size() >= WINDOW_SIZE)
            {
                /* send the packet */
                Sender_ToLowerLayer(&packet_buffer[9]);
                Sender_StartTimer(TIME_OUT_VALUE);
                // fprintf(stdout, "At %.2fs: sender sending packet %d ...\n", GetSimulationTime(), *(int *)(packet_buffer[0].data + 1));
            }
            else
            {
                packet_in_window--;
            }

            break;
        }
    }

    send_mutex.unlock();
    // fprintf(stdout, "At %.2fs: sender unlock in Sender_FromLowerLayer\n", GetSimulationTime());
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    send_mutex.lock();
    // fprintf(stdout, "At %.2fs: sender lock in Sender_Timeout\n", GetSimulationTime());

    /* resend all packets in the window */
    for (int i = 0; i < packet_in_window; i++)
    {
        Sender_ToLowerLayer(&packet_buffer[i]);
        Sender_StartTimer(TIME_OUT_VALUE);
        // fprintf(stdout, "At %.2fs: sender resending packet %d ...\n", GetSimulationTime(), *(int *)(packet_buffer[i].data + 1));
    }

    send_mutex.unlock();
    // fprintf(stdout, "At %.2fs: sender unlock in Sender_Timeout\n", GetSimulationTime());
}
