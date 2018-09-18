/**
 * Simple WebRTC test client.
 */

#include "WebSocketWrapper.hpp"
#include "json/json.h"
/* added by yaokabin 20180912 */
#include "serial/serial.h"
#include "autolabor/autolabor.h"
/* end */

#include <rtcdcpp/PeerConnection.hpp>
#include <rtcdcpp/Logging.hpp>

#include <ios>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>

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

void print_hex(uint8_t* data, int length) {
  for (int i = 0; i < length; i++) {
    std::cout << (*(data + i)) + 0 << " ";
  }
  std::cout << std::endl;
}
/* end */

int main() {
#ifndef SPDLOG_DISABLED
  auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  // spdlog::create("rtcdcpp.PeerConnection", console_sink);
  // spdlog::create("rtcdcpp.SCTP", console_sink);
  // spdlog::create("rtcdcpp.Nice", console_sink);
  // spdlog::create("rtcdcpp.DTLS", console_sink);
  spdlog::set_level(spdlog::level::debug);
#endif
  /* added by yaokaibin 2018091 */
  const char* serialName = "/dev/ttyUSB0";
  Autolabor autolabor;
  serial serial;
  char result = serial.Open(serialName, 115200, 8, NO, 1);
  if (result == 0) {
    std::cout << "fail to open serial port: " << serialName << std::endl;
    return 0;
  }
  /* end */

  WebSocketWrapper ws("ws://10.170.201.19:8443");
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
  std::function<void(std::string)> onDCRecvStrMsg = [&autolabor, &serial](std::string msg) {
    std::cout << "DataChannel receive string message: " << msg << std::endl;
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(msg, root)) {
      if (root["op"].asString() == "publish" && root["topic"].asString() == "/cmd_vel") {
        std::string linear_speed_str = root["msg"]["linear"]["x"].asString();
        std::string angular_speed_str = root["msg"]["angular"]["z"].asString();
        double linear_speed = atof(linear_speed_str.c_str());
        double angular_speed = atof(angular_speed_str.c_str());
        std::cout << "linear_speed: " << linear_speed << ", angular_speed: " << angular_speed << std::endl;

        uint8_t* reset_cmd = autolabor.CreateResetCmd();
        print_hex(reset_cmd, autolabor.RESET_CMD_LENGTH);
        char result = serial.Write(reset_cmd, autolabor.RESET_CMD_LENGTH);
        if (result == 0) {
          std::cout << "write reset cmd to serial failed" << std::endl;
        }
        delete reset_cmd;

        uint8_t* speed_cmd = autolabor.CreateSpeedCmd(linear_speed, angular_speed);
        print_hex(speed_cmd, autolabor.SPEED_CMD_LENGTH);
        result = serial.Write(speed_cmd, autolabor.SPEED_CMD_LENGTH);
        if (result == 0) {
          std::cout << "write speed cmd to serial failed" << std::endl;
        }
        delete speed_cmd;
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
      std::cout << "Registered with server" << std::endl;
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
