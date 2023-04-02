#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/database.grpc.pb.h"
#else
#include "database.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using database::Database;
using database::DatabaseRequest;
using database::DatabaseResponse;

class DatabaseClient {
 private:
  uint32_t sequenceID;
 public:
  DatabaseClient(std::shared_ptr<Channel> channel)
      : stub_(Database::NewStub(channel)),
        sequenceID(0) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  int get(const std::string key) {
    std::cout << "Entering Get\n";
    DatabaseRequest request;
    request.set_sequenceid(sequenceID);
    request.set_datakey(key);

    DatabaseResponse reply;

    ClientContext context;
    printRequest(&request);
    Status status = stub_->Get(&context, request, &reply);
    printResponse(&reply);
    std::cout << "Exiting Get\n";
    // Act upon its status.
    if (status.ok()) {
      return reply.datavalue();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

  int put(const std::string key, const int value) {
    std::cout << "Entering Put\n";
    DatabaseRequest request;
    request.set_sequenceid(sequenceID);
    request.set_datakey(key);
    request.set_datavalue(value);

    DatabaseResponse reply;

    ClientContext context;
    printRequest(&request);
    Status status = stub_->Put(&context, request, &reply);
    printResponse(&reply);
    std::cout << "Exiting Put\n";

    // Act upon its status.
    if (status.ok()) {
      return reply.datavalue();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

  void printRequest(DatabaseRequest* request) {
      std::cout <<" Printing Request : \n";
      std::cout << "SequenceID : " << request->sequenceid() << ", dataKey : " <<request->datakey() << ", dataValue : " << request->datavalue() << std::endl;
  }

  void printResponse(DatabaseResponse* reply) {
      std::cout <<" Printing Response : \n";
      std::cout << "success : " << reply->success() << ", leaderID : " <<reply->leaderid() << ", dataValue : " << reply->datavalue() << std::endl;
  }

 private:
  std::unique_ptr<Database::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_str = "10.10.1.3:50051";
  DatabaseClient client(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  int value;
  // Commands go here
//  client.put("a", 10);
//  client.put("b", 15);
//  client.put("c", 20);
//  client.put("d", 25);
  value = client.get("b");
  std::cout << "b = " << value;
  value = client.get("d");
  std::cout << "d = " << value;
  value = client.get("a");
  std::cout << "a = " << value;
  value = client.get("c");
  std::cout << "c = " << value;
  value = client.get("f");
  std::cout << "f = " << value;
  value = client.get("e");
  std::cout << "e = " << value;

  return 0;
}
