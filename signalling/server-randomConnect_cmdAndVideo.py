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
parser.add_argument('--addr', default='193.168.100.231', help='Address to listen on')
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
peers_car = dict()
peers_controller = dict()
# Format: {caller_uid: callee_uid,
#          callee_uid: caller_uid}
# Bidirectional mapping between the two peers
sessions = dict()

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
    global sessions
    if peer_id in sessions:
        del sessions[peer_id]
        print("Cleaned up {} session, in disconnect function".format(uid))
    # Close connection
    if ws and ws.open:
        # Don't care about errors
        asyncio.ensure_future(ws.close(reason='hangup'))
        print("Closing connection to {}, in disconnect function".format(uid))

async def cleanup_session(msg):
    classes, hello, uid = msg.split(maxsplit=2)
    if uid in sessions:
        other_id = sessions[uid]
        del sessions[uid]
        print("Cleaned up {} session".format(uid))
        if other_id in sessions:
            del sessions[other_id]
            print("Also cleaned up {} session".format(other_id))
            # If there was a session with this peer, also
            # close the connection to reset its state.
            '''if other_id in peers_controller:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_controller[other_id]
                del peers_controller[other_id]
                await wso.close()
            else:
                print("Closing connection to {}".format(other_id))
                wso, oaddr, _ = peers_car[other_id]
                del peers_car[other_id]
                await wso.close()'''
            if other_id in peers_car:
                video_wso, video_oaddr, data_wso, data_oaddr, _ = peers_car[other_id]
                del peers_car[other_id]
                print("Closing video connection to {}".format(other_id))
                await video_wso.close()
                print("Closing data connection to {}".format(other_id))
                await data_wso.close()

async def remove_peer(msg):
    await cleanup_session(msg)
    classes, hello, uid = msg.split(maxsplit=2)
    if classes == 'Car_data' or classes == 'Car_video': 
        if uid in peers_car:
            video_ws, video_raddr, data_ws, data_raddr, status = peers_car[uid]
            del peers_car[uid]
            await video_ws.close()
            await data_ws.close()
            print("Disconnected from peer {!r} at video_raddr: {!r} and data_raddr: {!r}".format(uid, video_raddr, data_raddr))
    else: 
        if uid in peers_controller:
            video_ws, video_raddr, data_ws, data_raddr, status = peers_controller[uid]
            del peers_controller[uid]
            await video_ws.close()
            await data_ws.close()
            print("Disconnected from peer {!r} at video_raddr: {!r} and data_raddr: {!r}".format(uid, video_raddr, data_raddr))

############### Handler functions ###############

async def connection_handler(ws, msg):
    global peers_car, peers_controller, sessions
    raddr = ws.remote_address
    peer_status = None

    # classes : car or controller
    # uid: car or controller id
    # hello: message type

    classes, hello, uid = msg.split(maxsplit=2)

    #peers_car[uid] = [video_ws, video_raddr, data_ws, data_raddr, peer_status]
    if classes == 'Car_video':
        data_ws = peers_car[uid][2]
        data_raddr = peers_car[uid][3]
        peers_car[uid] = [ws, raddr, data_ws, data_raddr, peer_status] 
    elif classes == 'Car_data':
        video_ws = peers_car[uid][0]
        video_raddr = peers_car[uid][1]
        peers_car[uid] = [video_ws, video_raddr, ws, raddr, peer_status]
    elif classes == 'Controller_data':
        video_ws = peers_controller[uid][0]
        video_raddr = peers_controller[uid][1]
        peers_controller[uid] = [video_ws, video_raddr, ws, raddr, peer_status]
    else:
        data_ws = peers_controller[uid][2]
        data_raddr = peers_controller[uid][3]
        peers_controller[uid] = [ws, raddr, data_ws, data_raddr, peer_status]
    print("{!r} registered peer {!r} at {!r}".format(classes, uid, raddr))
    while True:
        # Receive command, wait forever if necessary
        msg = await recv_msg_ping(ws, raddr)
        # Update current status
        if classes == 'Car_video' or classes == 'Car_data':
            peer_status = peers_car[uid][4]
        else:
            peer_status = peers_controller[uid][4]

	##############################################################################################
        # We are in a session or a room, messages must be relayed
        if peer_status is not None:
            # We're in a session, route message to connected peer
            if peer_status == 'session':
                other_id = sessions[uid]
                if classes == 'Car_video' or classes == 'Car_data':
                    video_wso, video_oaddr, data_wso, data_oaddr, status = peers_controller[other_id]
                else:
                    video_wso, video_oaddr, data_wso, data_oaddr, status = peers_car[other_id]
                assert(status == 'session')
                if classes == 'Car_video' or classes == 'Controller_video':
                    print("Video {} -> {}: {}".format(uid, other_id, msg))
                    await video_wso.send(msg)
                else:
                    print("Data {} -> {}: {}".format(uid, other_id, msg))
                    await data_wso.send(msg)
            else:
                raise AssertionError('Unknown peer status {!r}'.format(peer_status))
        # Requested a session with a specific peer
        # modified by liyujia, requested a session
        elif msg.startswith('Sessions'):
            video_wso = peers_car[uid][0]
            video_oaddr = peers_car[uid][1]
            if video_wso is not None and video_oaddr is not None:
                print("{!r} command {!r}".format(uid, msg))
                callee_id = None
                for (_id, _vlaue) in peers_controller.items():
                    if peers_controller[_id][4] != 'session':
                        callee_id = _id
                #peers_car[_id][2] = 'session'
                        break
            
                await ws.send('Sessions OK')
                await video_wso.send('Sessions OK')
                video_wsc = peers_controller[callee_id][0]
                data_wsc = peers_controller[callee_id][2]
                print('Data Session from {!r} ({!r}) to {!r} ({!r}) '.format(uid, raddr, callee_id, data_wsc.remote_address))
                print('Video Session from {!r} ({!r}) to {!r} ({!r}) '.format(uid, video_wso.remote_address, callee_id, video_wsc.remote_address))
                
                # Register session
                peers_car[uid][4] = peer_status = 'session'
                sessions[uid] = callee_id
                peers_controller[callee_id][4] = 'session'
                sessions[callee_id] = uid
            else:
                await ws.send('Sessions failed, your Car_video is not registered yet!')
            
        else:
            print('Ignoring unknown message {!r} from {!r}'.format(msg, uid))

async def hello_peer(ws):
    '''
    Exchange hello, register peer
    '''
    raddr = ws.remote_address
    helloMsg = await ws.recv()
    print('{!r}'.format(helloMsg))

    # classes : Car_video or Car_data or Controller_video or Controller_data
    # uid1: car or controller id
    # hello: message type
    # uid2: video or data id

    classes, hello, uid = helloMsg.split(maxsplit=2)

    if hello != 'hello':
        await ws.close(code=1002, reason='invalid protocol')
        raise Exception("Invalid hello from {!r}".format(raddr))
    if not uid1 or uid1 in peers_car or uid1 in peers_controller or uid1.split() != [uid1]: # no whitespace
        await ws.close(code=1002, reason='invalid peer uid')
        raise Exception("Invalid uid1 {!r} from {!r}".format(uid1, raddr))
    # Send back a HELLO
    if classes == 'Car_video':
        await ws.send('Hello Car_video')
    elif classes == 'Car_data':
        await ws.send('Hello Car_data')
    elif classes == 'Controller_data':
        await ws.send('Hello Controller_data')
    elif classes == 'Controller_video':
        await ws.send('Hello Controller_video')
    else:
        await ws.send('Hello, I do not know who you are!')
    return helloMsg

async def handler(ws, path):
    '''
    All incoming messages are handled here. @path is unused.
    '''
    raddr = ws.remote_address
    print("Connected to {!r}".format(raddr))
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

print("Listening on https://{}:{}".format(*ADDR_PORT))
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
