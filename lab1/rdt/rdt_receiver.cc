/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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
#include "rdt_receiver.h"

// ------------------------- 全局变量 -------------------------
std::vector<packet> receiver_packet_buffer; // 数据包缓冲区
int expected_sequence_number = 0;           // 期望的数据包序列号
std::mutex receive_mutex;                   // 互斥锁

// ------------------------- 函数声明 -------------------------
std::hash<std::string> receiver_hash_fn; // hash 函数

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* get payload size, sequence number, and checksum */
    int payload_size = pkt->data[0];
    int sequence_number = *(int *)(pkt->data + 1);
    int checksum_got = *(int *)(pkt->data + 5);

    /* if payload size is smaller than 0 or larger than 119, ignore it */
    if (payload_size <= 0 || payload_size > 119)
    {
        // fprintf(stdout, "At %.2fs: receiver: payload size error\n", GetSimulationTime());
        return;
    }

    /* calculate the checksum */
    std::string payload = std::string(pkt->data + 9, payload_size);
    int checksum_cal = receiver_hash_fn(std::to_string(payload_size) + std::to_string(sequence_number) + payload);

    /* if the checksum is correct and the sequence number is expected */
    if (checksum_got == checksum_cal)
    {
        receive_mutex.lock();
        // fprintf(stdout, "At %.2fs: receiver: lock %d\n", GetSimulationTime(), sequence_number);

        /* send ack to the sender */
        struct packet ack_pkt;
        ack_pkt.data[0] = 0;
        memcpy(ack_pkt.data + 1, &sequence_number, 4);
        Receiver_ToLowerLayer(&ack_pkt);

        /* if sequence number is smaller than expected, ignore it */
        if (sequence_number < expected_sequence_number)
        {
            receive_mutex.unlock();
            // fprintf(stdout, "At %.2fs: receiver: unlock %d\n", GetSimulationTime(), sequence_number);
            return;
        }

        if (sequence_number == expected_sequence_number)
        {
            /* construct a message */
            struct message *msg = (struct message *)malloc(sizeof(struct message));
            ASSERT(msg != NULL);

            /* update the expected sequence number */
            expected_sequence_number++;

            /* copy the payload to the message */
            msg->size = payload_size;
            msg->data = (char *)malloc(msg->size);
            ASSERT(msg->data != NULL);
            memcpy(msg->data, pkt->data + 9, msg->size);

            /* deliver the message to the upper layer */
            Receiver_ToUpperLayer(msg);
            // fprintf(stdout, "At %.2fs: receiver: deliver packet %d\n", GetSimulationTime(), sequence_number);

            /* check if there are any packets in the buffer that can be delivered */
            for (auto it = receiver_packet_buffer.begin(); it < receiver_packet_buffer.end();)
            {
                /* get sequence number */
                int sequence_number = *(int *)(it->data + 1);

                /* if the sequence number is smaller than expected, delete it */
                if (sequence_number < expected_sequence_number)
                {
                    it = receiver_packet_buffer.erase(it);
                    continue;
                }

                /* if the sequence number is expected */
                if (sequence_number == expected_sequence_number)
                {
                    /* update the expected sequence number */
                    expected_sequence_number++;

                    /* copy the payload to the message */
                    msg->size = it->data[0];
                    free(msg->data);
                    msg->data = (char *)malloc(msg->size);
                    ASSERT(msg->data != NULL);
                    memcpy(msg->data, it->data + 9, msg->size);

                    /* deliver the message to the upper layer */
                    Receiver_ToUpperLayer(msg);
                    // fprintf(stdout, "At %.2fs: receiver: deliver packet %d\n", GetSimulationTime(), sequence_number);

                    /* remove the packet from the buffer */
                    receiver_packet_buffer.erase(it);

                    /* set the iterator to the beginning */
                    it = receiver_packet_buffer.begin();
                }
                else
                {
                    it++;
                }
            }

            /* don't forget to free the space */
            if (msg->data != NULL)
                free(msg->data);
            if (msg != NULL)
                free(msg);
        }
        else
        {
            /* save the packet in the buffer */
            receiver_packet_buffer.push_back(*pkt);
            // fprintf(stdout, "At %.2fs: receiver: buffer packet %d\n", GetSimulationTime(), sequence_number);
        }

        receive_mutex.unlock();
        // fprintf(stdout, "At %.2fs: receiver: unlock %d\n", GetSimulationTime(), sequence_number);
    }
    else
    {
        /* if the checksum is incorrect */
        // fprintf(stdout, "At %.2fs: receiver: checksum error\n", GetSimulationTime());
    }
}
