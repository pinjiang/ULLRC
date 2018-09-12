/**
 * Simple WebRTC test client.
 */

#include "WebSocketWrapper.hpp"
#include "json/json.h"

#include <rtcdcpp/PeerConnection.hpp>
#include <rtcdcpp/Logging.hpp>

#include <ios>
#include <iostream>
#include <fstream>

using namespace rtcdcpp;

void send_loop(std::shared_ptr<DataChannel> dc) {
  std::ifstream bunnyFile;
  bunnyFile.open("frag_bunny.mp4", std::ios_base::in | std::ios_base::binary);

  char buf[100 * 1024];

  while (bunnyFile.good()) {
    bunnyFile.read(buf, 100 * 1024);
    int nRead = bunnyFile.gcount();
    if (nRead > 0) {
      dc->SendBinary((const uint8_t *)buf, nRead);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Sent message of size " << std::to_string(nRead) << std::endl;
  }
}

std::string createId() {
  return "220";
}

int main() {
#ifndef SPDLOG_DISABLED
  auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  // spdlog::create("rtcdcpp.PeerConnection", console_sink);
  // spdlog::create("rtcdcpp.SCTP", console_sink);
  // spdlog::create("rtcdcpp.Nice", console_sink);
  // spdlog::create("rtcdcpp.DTLS", console_sink);
  spdlog::set_level(spdlog::level::debug);
#endif

  // WebSocketWrapper ws("ws://localhost:5000/channel/test");
  WebSocketWrapper ws("ws://122.112.211.178:8443");
  std::shared_ptr<PeerConnection> pc;
  std::shared_ptr<DataChannel> dc;

  if (!ws.Initialize()) {
    std::cout << "WebSocket connection failed\n";
    return 0;
  }

  RTCConfiguration config;
  config.ice_servers.emplace_back(RTCIceServer{"stun3.l.google.com", 19302});

  bool running = true;

  ChunkQueue messages;

  std::function<void(std::string)> onMessage = [&messages](std::string msg) {
    messages.push(std::shared_ptr<Chunk>(new Chunk((const void *)msg.c_str(), msg.length())));
  };

  std::function<void(PeerConnection::IceCandidate)> onLocalIceCandidate = [&ws](PeerConnection::IceCandidate candidate) {
    Json::Value jsonCandidate;
    jsonCandidate["type"] = "candidate";
    jsonCandidate["msg"]["candidate"] = candidate.candidate;
    jsonCandidate["msg"]["sdpMid"] = candidate.sdpMid;
    jsonCandidate["msg"]["sdpMLineIndex"] = candidate.sdpMLineIndex;

    Json::StreamWriterBuilder wBuilder;
    ws.Send(Json::writeString(wBuilder, jsonCandidate));
  };

  std::function<void(std::shared_ptr<DataChannel> channel)> onDataChannel = [&dc](std::shared_ptr<DataChannel> channel) {
    std::cout << "Hey cool, got a data channel\n";
    dc = channel;
    std::thread send_thread = std::thread(send_loop, channel);
    send_thread.detach();
  };

  /* added by yaokaibin 20180911 */
  std::function<void(std::string)> onDCRecvStrMsg = [](std::string msg) {
    std::cout << "DataChannel receive string message: " << msg << std::endl;
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(msg, root)) {
      if (root["op"].asString() == "publish" && root["topic"].asString() == "/cmd_vel") {
        std::string linearX = root["msg"]["linear"]["x"].asString();
        std::string angularZ = root["msg"]["angular"]["z"].asString();
        std::cout << "linearX: " << linearX << ", angularZ: " << angularZ << std::endl;
      }
    }
  };

  std::function<void(ChunkPtr)> onDCRecvBinMsg = [](ChunkPtr msg) {
    std::cout << "DataChannel receive binary message" << std::endl;
  };
  /* end */

  ws.SetOnMessage(onMessage);
  ws.Start();
  // ws.Send("{\"type\": \"client_connected\", \"msg\": {}}");

  Json::Reader reader;
  Json::StreamWriterBuilder msgBuilder;

  while (running) {
    ChunkPtr cur_msg = messages.wait_and_pop();
    std::string msg((const char *)cur_msg->Data(), cur_msg->Length());
    std::cout << msg << "\n";
    if (msg == "hello")
    {
      std::cout << "received server's ack" << std::endl;
      ws.Send("Car_data hello " + createId());
      continue;
    }
    if (msg == "Hello Car_data") {
      std::cout << "Hello from websocket server" << std::endl;
      continue;
    }
    Json::Value root;
    if (reader.parse(msg, root)) {
      std::cout << "Got msg of type: " << root["type"] << "\n";
      if (root["type"] == "offer") {
        std::cout << "Time to get the rtc party started\n";
        pc = std::make_shared<PeerConnection>(config, onLocalIceCandidate, onDataChannel, onDCRecvStrMsg, onDCRecvBinMsg);

        pc->ParseOffer(root["msg"]["sdp"].asString());
        Json::Value answer;
        answer["type"] = "answer";
        answer["msg"]["sdp"] = pc->GenerateAnswer();
        answer["msg"]["type"] = "answer";

        std::cout << "Sending Answer: " << answer << "\n";
        ws.Send(Json::writeString(msgBuilder, answer));
      } else if (root["type"] == "candidate") {
        pc->SetRemoteIceCandidate("a=" + root["msg"]["candidate"].asString());
      }
    } else {
      std::cout << "Json parse failed"
                << "\n";
    }
  }

  ws.Close();

  return 0;
}
