#include "common.h"

int main()
{
    char buffer[PACKET_LEN];
    memset(buffer, 0, PACKET_LEN);

    ipheader *ip = (ipheader *)buffer;
    udpheader *udp = (udpheader *)(buffer + sizeof(ipheader));

    // add data
    char *data = (char *)udp + sizeof(udpheader);
    int data_len = strlen(CLIENT_IP);
    strncpy(data, CLIENT_IP, data_len);
    /**********************************************************************/
    // create udp header
    // TODO
    udp->udp_sport = htons(CLIENT_PORT); // source
    udp->udp_dport = htons(SERVER_PORT); // destination
    udp->udp_ulen = htons(sizeof(udpheader) + data_len); // udp length
    udp->udp_sum = 0;
    // udp is the protocol

    // create ip header
    ip->iph_ver = 4; // ip version (IPv4)
    ip->iph_ihl = 5; // ip header 
    ip->iph_ttl = 20; // time to live
    ip->iph_sourceip.s_addr = inet_addr(SPOOF_IP); // IP Source address (In network byte order)
    ip->iph_destip.s_addr = inet_addr(SERVER_IP); // IP Destination address (In network byte order)
    ip->iph_protocol = IPPROTO_UDP; // Type of the upper-level protocol
    ip->iph_len = htons(sizeof(ipheader) + sizeof(udpheader) + data_len); // IP Packet length (Both data and header)

    // send packet
    send_raw_ip_packet(ip);
    
    /**********************************************************************/
    return 0;
}