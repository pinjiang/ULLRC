#!/usr/bin/env python3
#
# Example 1-1 call signalling server
#
# Copyright (C) 2017 Centricular Ltd.
#
#  Author: Nirbheek Chauhan <nirbheek@centricular.com>
#

import os
import sys
import ssl
import logging
import asyncio
import websockets
import argparse

from concurrent.futures._base import TimeoutError

parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--addr', default='0.0.0.0', help='Address to listen on')
parser.add_argument('--port', default=8443, type=int, help='Port to listen on')
parser.add_argument('--keepalive-timeout', dest='keepalive_timeout', default=30, type=int, help='Timeout for keepalive (in seconds)')
parser.add_argument('--cert-path', default=os.path.dirname(__file__))
parser.add_argument('--disable-ssl', default=False, help='Disable ssl', action='store_true')

options = parser.parse_args(sys.argv[1:])

ADDR_PORT = (options.addr, options.port)
KEEPALIVE_TIMEOUT = options.keepalive_timeout

############### Global data ###############

# Format: {uid: (Peer WebSocketServerProtocol,
#                remote_address,
#                <'session'|room_id|None>)}
peers_car_video = dict()
peers_car_data = dict()
peers_controller_video = dict()
peers_controller_data = dict()
# Format: {caller_uid: callee_uid,
#          callee_uid: caller_uid}
# Bidirectional mapping between the two peers
sessions_video = dict()
sessions_data = dict()

############### Helper functions ###############

async def recv_msg_ping(ws, raddr):
    '''
    Wait for a message forever, and send a regular ping to prevent bad routers
    from closing the connection.
    '''
    msg = None
    while msg is None:
        try:
            msg = await asyncio.wait_for(ws.recv(), KEEPALIVE_TIMEOUT)
        except TimeoutError:
            print('Sending keepalive ping to {!r} in recv'.format(raddr))
            await ws.ping()
    return msg

async def disconnect(ws, peer_id):
    '''
    Remove @peer_id from the list of sessions and close our connection to it.
    This informs the peer that the session and all calls have ended, and it
    must reconnect.
    '''
    global sessions_data, sessions_video
    if peer_id in sessions_data:
        del sessions_data[peer_id]
    elif peer_id in sessions_video:
        del sessions_video[peer_id]
    # Close connection
    if ws and ws.open:
        # Don't care about errors
        asyncio.ensure_future(ws.close(reason='hangup'))

async def cleanup_session(msg):
    classes, hello, uid = msg.split(maxsplit=2)
    if uid in sessions_data:
        other_id = sessions_data[uid]
        del sessions_data[uid]
        print("Cleaned up {} data session".format(uid))
        if other_id in sessions_data:
            del sessions_data[other_id]
            print("Also cleaned up {} data session".format(other_id))
            # If there was a session with this peer, also
            # close the connection to reset its state.
            if other_id in peers_controller_data:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_controller_data[other_id]
                del peers_controller_data[other_id]
                await wso.close()
            else:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_car_data[other_id]
                del peers_car_data[other_id]
                await wso.close()
        elif other_id in sessions_video:
            del sessions_video[other_id]
            print("Also cleaned up {} video session".format(other_id))
            # If there was a session with this peer, also
            # close the connection to reset its state.
            if other_id in peers_controller_video:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_controller_video[other_id]
                del peers_controller_video[other_id]
                await wso.close()
            else:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_car_video[other_id]
                del peers_car_video[other_id]
                await wso.close()

async def remove_peer(msg):
    await cleanup_session(msg)
    ws = None
    raddr = None
    status = None
    classes, hello, uid = msg.split(maxsplit=2)
    if classes == 'Car_data': 
        if uid in peers_car_data:
            ws, raddr, status = peers_car_data[uid]
            del peers_car_data[uid]
    elif classes == 'Controller_data': 
        if uid in peers_controller_data:
            ws, raddr, status = peers_controller_data[uid]
            del peers_controller_data[uid]
    elif classes == 'Controller_video':
        if uid in peers_controller_video:
            ws, raddr, status = peers_controller_video[uid]
            del peers_controller_video[uid]
    else:
        if uid in peers_car_video:
            ws, raddr, status = peers_car_video[uid]
            del peers_car_video[uid]
    await ws.close()
    print("Disconnected from peer {!r} at {!r}".format(uid, raddr))

############### Handler functions ###############

async def connection_handler(ws, msg):
    global peers_car_data, peers_car_video, peers_controller_data, peers_controller_video, sessions_video, sessions_data
    raddr = ws.remote_address
    peer_status = None
    classes, hello, uid = msg.split(maxsplit=2)
    if classes == 'Controller_data':
        peers_controller_data[uid] = [ws, raddr, peer_status]
    elif classes == 'Controller_video':
        peers_controller_video[uid] = [ws, raddr, peer_status]
    elif classes == 'Car_data':
        peers_car_data[uid] = [ws, raddr, peer_status]
    else:
        peers_car_video[uid] = [ws, raddr, peer_status]
    print("{!r} registered peer {!r} at {!r}".format(classes, uid, raddr))
    while True:
        # Receive command, wait forever if necessary
        msg = await recv_msg_ping(ws, raddr)
        # Update current status
        if classes == 'Car_data':
            peer_status = peers_car_data[uid][2]
        elif classes == 'Car_video':
            peer_status = peers_car_video[uid][2]
        elif classes == 'Controller_data':
            peer_status = peers_controller_data[uid][2]
        else:
            peer_status = peers_controller_video[uid][2]

	##############################################################################################
        # We are in a session , messages must be relayed
        # modified by liyujia, 20180911
        if peer_status is not None:
            # We're in a session, route message to connected peer
            if peer_status == 'session':
                if classes == 'Car_video' or classes == 'Controller_video':
                    other_id = sessions_video[uid]
                else:
                    other_id = sessions_data[uid]
                if classes == 'Controller_data':
                    wso, oaddr, status = peers_car_data[other_id]
                elif classes == 'Controller_video':
                    wso, oaddr, status = peers_car_video[other_id]
                elif classes == 'Car_data':
                    wso, oaddr, status = peers_controller_data[other_id]
                else:
                    wso, oaddr, status = peers_controller_video[other_id]
                assert(status == 'session')
                print("{} -> {}: {}".format(uid, other_id, msg))
                await wso.send(msg)
            else:
                raise AssertionError('Unknown peer status {!r}'.format(peer_status))
        # Requested a session with a specific peer
        # modified by liyujia, requested a session
        elif msg.startswith('Session'):
            print("{!r} command {!r}".format(uid, msg))
            callee_id = None
            wsc = None
            if classes == 'Car_video':
                for (_id, _vlaue) in peers_controller_video.items():
                    if peers_controller_video[_id][2] != 'session':
                        callee_id = _id
                        wsc = peers_controller_video[callee_id][0]
                        await ws.send('SESSION_OK')   # added by liyujia: send 'session ok' to caller
                        # Register session
                        peers_car_video[uid][2] = peer_status = 'session'
                        sessions_video[uid] = callee_id
                        peers_controller_video[callee_id][2] = 'session'
                        sessions_video[callee_id] = uid
                        break
            elif classes == 'Car_data':
                for (_id, _vlaue) in peers_controller_data.items():
                    if peers_controller_data[_id][2] != 'session':
                        callee_id = _id
                        wsc = peers_controller_data[callee_id][0]
                        await ws.send('SESSION_OK')
                        # Register session
                        peers_car_data[uid][2] = peer_status = 'session'
                        sessions_data[uid] = callee_id
                        peers_controller_data[callee_id][2] = 'session'
                        sessions_data[callee_id] = uid
                        break
            elif classes == 'Controller_video':
                for (_id, _vlaue) in peers_car_video.items():
                    if peers_car_video[_id][2] != 'session':
                        callee_id = _id
                        wsc = peers_car_video[callee_id][0]
                        await wsc.send('SESSION_OK')    # added by liyujia: we should send 'session ok' to car, because sdp message send by the controller is incomplete
                        # Register session
                        peers_controller_video[uid][2] = peer_status = 'session'
                        sessions_video[uid] = callee_id
                        peers_car_video[callee_id][2] = 'session'
                        sessions_video[callee_id] = uid
                        break
            else:
                for (_id, _vlaue) in peers_car_data.items():
                    if peers_car_data[_id][2] != 'session':
                        callee_id = _id
                        wsc = peers_car_data[callee_id][0]
                        await ws.send('SESSION_OK')
                        # Register session
                        peers_controller_data[uid][2] = peer_status = 'session'
                        sessions_data[uid] = callee_id
                        peers_car_data[callee_id][2] = 'session'
                        sessions_data[callee_id] = uid
                        break
	    
            print('Session from {!r} ({!r}) to {!r} ({!r})'
                  ''.format(uid, raddr, callee_id, wsc.remote_address))
        else:
            print('Ignoring unknown message {!r} from {!r}'.format(msg, uid))

async def hello_peer(ws):
    '''
    Exchange hello, register peer
    '''
    raddr = ws.remote_address
    helloMsg = await ws.recv()
    print('{!r}'.format(helloMsg))
    classes, hello, uid = helloMsg.split(maxsplit=2)

    if hello != 'hello':
        await ws.close(code=1002, reason='invalid protocol')
        raise Exception("Invalid hello from {!r}".format(raddr))
    if classes == 'Controller_data':
        if not uid or uid in peers_controller_data or uid.split() != [uid]:
            await ws.close(code=1002, reason='invalid peer uid')
            raise Exception("Invalid uid {!r} from {!r}".format(uid, raddr))
        else:
            await ws.send('Hello Controller_data')
    elif classes == 'Controller_video':
        if not uid or uid in peers_controller_video or uid.split() != [uid]:
            await ws.close(code=1002, reason='invalid peer uid')
            raise Exception("Invalid uid {!r} from {!r}".format(uid, raddr))
        else:
            await ws.send('Hello Controller_video')
    elif classes == 'Car_data':
        if not uid or uid in peers_car_data or uid.split() != [uid]:
            await ws.close(code=1002, reason='invalid peer uid')
            raise Exception("Invalid uid {!r} from {!r}".format(uid, raddr))
        else:
            await ws.send('Hello Car_data')
    elif classes == 'Car_video':
        if not uid or uid in peers_car_video or uid.split() != [uid]:
            await ws.close(code=1002, reason='invalid peer uid')
            raise Exception("Invalid uid {!r} from {!r}".format(uid, raddr))
        else:
            await ws.send('Hello Car_video')
    else:
        await ws.close(code=1002, reason='invalid protocol')
        raise Exception("Invalid classes from {!r}".format(raddr))
    return helloMsg

async def handler(ws, path):
    '''
    All incoming messages are handled here. @path is unused.
    '''
    raddr = ws.remote_address
    print("Connected to {!r}".format(raddr))
    await ws.send('hello')
    helloMsg = await hello_peer(ws)
    try:
        await connection_handler(ws, helloMsg)
    except websockets.ConnectionClosed:
        print("Connection to peer {!r} closed, exiting handler".format(raddr))
    finally:
        await remove_peer(helloMsg)

sslctx = None
if not options.disable_ssl:
    # Create an SSL context to be used by the websocket server
    certpath = options.cert_path
    print('Using TLS with keys in {!r}'.format(certpath))
    if 'letsencrypt' in certpath:
        chain_pem = os.path.join(certpath, 'fullchain.pem')
        key_pem = os.path.join(certpath, 'privkey.pem')
    else:
        chain_pem = os.path.join(certpath, 'cert.pem')
        key_pem = os.path.join(certpath, 'key.pem')

    sslctx = ssl.create_default_context()
    try:
        sslctx.load_cert_chain(chain_pem, keyfile=key_pem)
    except FileNotFoundError:
        print("Certificates not found, did you run generate_cert.sh?")
        sys.exit(1)
    # FIXME
    sslctx.check_hostname = False
    sslctx.verify_mode = ssl.CERT_NONE

print("Listening on http://{}:{}".format(*ADDR_PORT))
# Websocket server
wsd = websockets.serve(handler, *ADDR_PORT, ssl=sslctx,
                       # Maximum number of messages that websockets will pop
                       # off the asyncio and OS buffers per connection. See:
                       # https://websockets.readthedocs.io/en/stable/api.html#websockets.protocol.WebSocketCommonProtocol
                       max_queue=16)

logger = logging.getLogger('websockets.server')

logger.setLevel(logging.ERROR)
logger.addHandler(logging.StreamHandler())

asyncio.get_event_loop().run_until_complete(wsd)
asyncio.get_event_loop().run_forever()
