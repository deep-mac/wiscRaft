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
    DatabaseRequest request;
    request.set_sequenceid(sequenceID);
    request.set_datakey(key);

    DatabaseResponse reply;

    ClientContext context;

    Status status = stub_->Get(&context, request, &reply);

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
    DatabaseRequest request;
    request.set_sequenceid(sequenceID);
    request.set_datakey(key);
    request.set_datavalue(value);

    DatabaseResponse reply;

    ClientContext context;

    Status status = stub_->Put(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.datavalue();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

 private:
  std::unique_ptr<Database::Stub> stub_;
};

int main(int argc, char** argv) {
  std::string target_str = "10.10.1.2:50051";
  DatabaseClient client(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

  // Commands go here
  client.put("a", 10);
  client.get("a");
  return 0;
}
