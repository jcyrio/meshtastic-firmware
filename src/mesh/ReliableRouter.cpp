#include "ReliableRouter.h"
#include "Default.h"
#include "modules/RoutingModule.h"
#include "MeshModule.h"
#include "MeshTypes.h"
#include "configuration.h"
#include "mesh-pb-constants.h"
#ifdef SIMPLE_TDECK
// #include "graphics/Screen.h"  // for setDeliveryStatus
#include "modules/CannedMessageModule.h" // for setDeliveryStatus
// #include "main.h"
struct Message {
    PacketId id;
    NodeNum fromNode;
    NodeNum toNode;
};
Message* lastSentMessage = new Message();
extern CannedMessageModule* cannedMessageModule;
#endif

// ReliableRouter::ReliableRouter() {}

/**
 * If the message is want_ack, then add it to a list of packets to retransmit.
 * If we run out of retransmissions, send a nak packet towards the original client to indicate failure.
 */
ErrorCode ReliableRouter::send(meshtastic_MeshPacket *p)
{
    if (p->want_ack) {
        // If someone asks for acks on broadcast, we need the hop limit to be at least one, so that first node that receives our
        // message will rebroadcast.  But asking for hop_limit 0 in that context means the client app has no preference on hop
        // counts and we want this message to get through the whole mesh, so use the default.
        if (p->hop_limit == 0) {
            p->hop_limit = Default::getConfiguredOrDefaultHopLimit(config.lora.hop_limit);
        }

        auto copy = packetPool.allocCopy(*p);
        startRetransmission(copy);
    }

    /* If we have pending retransmissions, add the airtime of this packet to it, because during that time we cannot receive an
       (implicit) ACK. Otherwise, we might retransmit too early.
     */
    for (auto i = pending.begin(); i != pending.end(); i++) {
        if (i->first.id != p->id) {
            i->second.nextTxMsec += iface->getPacketTime(p);
        }
    }

    return FloodingRouter::send(p);
}

bool ReliableRouter::shouldFilterReceived(const meshtastic_MeshPacket *p)
{
    // Note: do not use getFrom() here, because we want to ignore messages sent from phone
    if (p->from == getNodeNum()) {
        printPacket("Rx someone rebroadcasting for us", p);

        // We are seeing someone rebroadcast one of our broadcast attempts.
        // If this is the first time we saw this, cancel any retransmissions we have queued up and generate an internal ack for
        // the original sending process.

        // This "optimization", does save lots of airtime. For DMs, you also get a real ACK back
        // from the intended recipient.
        auto key = GlobalPacketId(getFrom(p), p->id);
        auto old = findPendingPacket(key);
        if (old) {
            LOG_DEBUG("generating implicit ack\n");
            // NOTE: we do NOT check p->wantAck here because p is the INCOMING rebroadcast and that packet is not expected to be
            // marked as wantAck
            sendAckNak(meshtastic_Routing_Error_NONE, getFrom(p), p->id, old->packet->channel);

            stopRetransmission(key);
        } else {
            LOG_DEBUG("didn't find pending packet\n");
        }
    }

    /* At this point we have already deleted the pending retransmission if this packet was an (implicit) ACK to it.
       Now for all other pending retransmissions, we have to add the airtime of this received packet to the retransmission timer,
       because while receiving this packet, we could not have received an (implicit) ACK for it.
       If we don't add this, we will likely retransmit too early.
    */
    for (auto i = pending.begin(); i != pending.end(); i++) {
        i->second.nextTxMsec += iface->getPacketTime(p);
    }

    return FloodingRouter::shouldFilterReceived(p);
}

/**
 * If we receive a want_ack packet (do not check for wasSeenRecently), send back an ack (this might generate multiple ack sends in
 * case the our first ack gets lost)
 *
 * If we receive an ack packet (do check wasSeenRecently), clear out any retransmissions and
 * forward the ack to the application layer.
 *
 * If we receive a nak packet (do check wasSeenRecently), clear out any retransmissions
 * and forward the nak to the application layer.
 *
 * Otherwise, let superclass handle it.
 */
void ReliableRouter::sniffReceived(const meshtastic_MeshPacket *p, const meshtastic_Routing *c)
{
    NodeNum ourNode = getNodeNum();

    if (p->to == ourNode) { // ignore ack/nak/want_ack packets that are not address to us (we only handle 0 hop reliability)
        if (p->want_ack) {
            if (MeshModule::currentReply) {
                LOG_DEBUG("Some other module has replied to this message, no need for a 2nd ack\n");
            } else if (p->which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
                // A response may be set to want_ack for retransmissions, but we don't need to ACK a response if it received an
                // implicit ACK already. If we received it directly, only ACK with a hop limit of 0
                if (!p->decoded.request_id)
                    sendAckNak(meshtastic_Routing_Error_NONE, getFrom(p), p->id, p->channel,
                               routingModule->getHopLimitForResponse(p->hop_start, p->hop_limit));
                else if (p->hop_start > 0 && p->hop_start == p->hop_limit)
                    sendAckNak(meshtastic_Routing_Error_NONE, getFrom(p), p->id, p->channel, 0);

							
            } else {
                // Send a 'NO_CHANNEL' error on the primary channel if want_ack packet destined for us cannot be decoded
                sendAckNak(meshtastic_Routing_Error_NO_CHANNEL, getFrom(p), p->id, channels.getPrimaryIndex(),
										routingModule->getHopLimitForResponse(p->hop_start, p->hop_limit));
            }
        }

        // We consider an ack to be either a !routing packet with a request ID or a routing packet with !error
        PacketId ackId = ((c && c->error_reason == meshtastic_Routing_Error_NONE) || !c) ? p->decoded.request_id : 0;

        // A nak is a routing packt that has an  error code
        PacketId nakId = (c && c->error_reason != meshtastic_Routing_Error_NONE) ? p->decoded.request_id : 0;

        // We intentionally don't check wasSeenRecently, because it is harmless to delete non existent retransmission records
        if (ackId || nakId) {
            if (ackId) {
#ifdef SIMPLE_TDECK
							LOG_INFO("p->decoded.request_id: %x\n", p->decoded.request_id); //this is the id of the packet we sent
						  LOG_INFO("lastMessageID: %h\n", lastSentMessage->id); //this is the id of the packet we sent
							LOG_INFO("ackId: %x\n", ackId); //this is the id of the packet we setNextTx
							LOG_INFO("p->id: %x\n", p->id); //this is not the number we want
							LOG_INFO("LastMessageFrom: %x\n", lastSentMessage->fromNode);
							LOG_INFO("LastMessageTo: %x\n", lastSentMessage->toNode);
							LOG_INFO("deliveryStatus: %d\n", cannedMessageModule->getDeliveryStatus());
							//TODO: check make sure in logs lastNodeTo is not NODENUM_RPI5 (might want From instead)
							if ((ackId == lastSentMessage->id) && (lastSentMessage->fromNode == p->to) && (lastSentMessage->toNode == getFrom(p))) {
								//check if lastSentMessagee->id is empty
								if (cannedMessageModule->getDeliveryStatus() > 0) { // this is to check to see if we were actually trying to send a message. To prevent traceroutes etc from showing (D)
									cannedMessageModule->setDeliveryStatus(2);
								}
								// cannedMessageModule->setDeliveryStatus(2);
							}
#endif
                LOG_DEBUG("Received an ack for 0x%x, stopping retransmissions\n", ackId);
                stopRetransmission(p->to, ackId);
            } else {
                LOG_DEBUG("Received a nak for 0x%x, stopping retransmissions\n", nakId);
                stopRetransmission(p->to, nakId);
#ifdef SIMPLE_TDECK
								cannedMessageModule->setDeliveryStatus(0);
#endif
            }
        }
    }

    // handle the packet as normal
    FloodingRouter::sniffReceived(p, c);
}

#define NUM_RETRANSMISSIONS 3

PendingPacket::PendingPacket(meshtastic_MeshPacket *p)
{
    packet = p;
    numRetransmissions = NUM_RETRANSMISSIONS - 1; // We subtract one, because we assume the user just did the first send
}

PendingPacket *ReliableRouter::findPendingPacket(GlobalPacketId key)
{
    auto old = pending.find(key); // If we have an old record, someone messed up because id got reused
    if (old != pending.end()) {
        return &old->second;
    } else
        return NULL;
}
/**
 * Stop any retransmissions we are doing of the specified node/packet ID pair
 */
bool ReliableRouter::stopRetransmission(NodeNum from, PacketId id)
{
    auto key = GlobalPacketId(from, id);
    return stopRetransmission(key);
}

bool ReliableRouter::stopRetransmission(GlobalPacketId key)
{
    auto old = findPendingPacket(key);
    if (old) {
        auto p = old->packet;
        /* Only when we already transmitted a packet via LoRa, we will cancel the packet in the Tx queue
          to avoid canceling a transmission if it was ACKed super fast via MQTT */
        if (old->numRetransmissions < NUM_RETRANSMISSIONS - 1) {
            // remove the 'original' (identified by originator and packet->id) from the txqueue and free it
            cancelSending(getFrom(p), p->id);
        }
				// now free the pooled copy for retransmission too
				packetPool.release(p);
        auto numErased = pending.erase(key);
        assert(numErased == 1);
        return true;
    } else
        return false;
}

/**
 * Add p to the list of packets to retransmit occasionally.  We will free it once we stop retransmitting.
 */
PendingPacket *ReliableRouter::startRetransmission(meshtastic_MeshPacket *p)
{
    auto id = GlobalPacketId(p);
    auto rec = PendingPacket(p);

    stopRetransmission(getFrom(p), p->id);

    setNextTx(&rec);
    pending[id] = rec;

    return &pending[id];
}

/**
 * Do any retransmissions that are scheduled (FIXME - for the time being called from loop)
 */
int32_t ReliableRouter::doRetransmissions()
{
    uint32_t now = millis();
    int32_t d = INT32_MAX;

    // FIXME, we should use a better datastructure rather than walking through this map.
    // for(auto el: pending) {
    for (auto it = pending.begin(), nextIt = it; it != pending.end(); it = nextIt) {
        ++nextIt; // we use this odd pattern because we might be deleting it...
        auto &p = it->second;

        bool stillValid = true; // assume we'll keep this record around

        // FIXME, handle 51 day rolloever here!!!
        if (p.nextTxMsec <= now) {
            if (p.numRetransmissions == 0) {
                LOG_DEBUG("Reliable send failed, returning a nak for fr=0x%x,to=0x%x,id=0x%x\n", p.packet->from, p.packet->to,
                          p.packet->id);
                sendAckNak(meshtastic_Routing_Error_MAX_RETRANSMIT, getFrom(p.packet), p.packet->id, p.packet->channel);
                // Note: we don't stop retransmission here, instead the Nak packet gets processed in sniffReceived
                stopRetransmission(it->first);
                stillValid = false; // just deleted it
            } else {
                LOG_DEBUG("Sending reliable retransmission fr=0x%x,to=0x%x,id=0x%x, tries left=%d\n", p.packet->from,
                          p.packet->to, p.packet->id, p.numRetransmissions);

                // Note: we call the superclass version because we don't want to have our version of send() add a new
                // retransmission record
                FloodingRouter::send(packetPool.allocCopy(*p.packet));

                // Queue again
                --p.numRetransmissions;
                setNextTx(&p);
            }
        }

        if (stillValid) {
            // Update our desired sleep delay
            int32_t t = p.nextTxMsec - now;

            d = min(t, d);
        }
    }

    return d;
}

void ReliableRouter::setNextTx(PendingPacket *pending)
{
    assert(iface);
    auto d = iface->getRetransmissionMsec(pending->packet);
    pending->nextTxMsec = millis() + d;
    LOG_DEBUG("Setting next retransmission in %u msecs: ", d);
    printPacket("", pending->packet);
#ifdef SIMPLE_TDECK
		lastSentMessage->id = pending->packet->id;
		lastSentMessage->fromNode = getFrom(pending->packet);
		lastSentMessage->toNode = pending->packet->to;
#endif
    setReceivedMessage(); // Run ASAP, so we can figure out our correct sleep time
}
