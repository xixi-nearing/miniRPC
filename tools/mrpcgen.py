#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
from dataclasses import dataclass


TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*|\d+|[{}();=.]")


TYPE_MAP = {
    "bool": ("bool", "WriteIntegral", "ReadIntegral", "false"),
    "int32": ("std::int32_t", "WriteIntegral", "ReadIntegral", "0"),
    "int64": ("std::int64_t", "WriteIntegral", "ReadIntegral", "0"),
    "uint32": ("std::uint32_t", "WriteIntegral", "ReadIntegral", "0"),
    "uint64": ("std::uint64_t", "WriteIntegral", "ReadIntegral", "0"),
    "double": ("double", "WriteDouble", "ReadDouble", "0.0"),
    "string": ("std::string", "WriteString", "ReadString", "{}"),
}


@dataclass
class Field:
    field_type: str
    name: str
    tag: int


@dataclass
class Message:
    name: str
    fields: list[Field]


@dataclass
class RpcMethod:
    name: str
    request_type: str
    response_type: str


@dataclass
class Service:
    name: str
    methods: list[RpcMethod]


class Parser:
    def __init__(self, text: str):
        self.tokens = TOKEN_RE.findall(text)
        self.index = 0

    def peek(self) -> str | None:
        if self.index >= len(self.tokens):
            return None
        return self.tokens[self.index]

    def pop(self, expected: str | None = None) -> str:
        token = self.peek()
        if token is None:
            raise ValueError("unexpected end of file")
        if expected is not None and token != expected:
            raise ValueError(f"expected '{expected}', got '{token}'")
        self.index += 1
        return token

    def parse(self) -> tuple[str, list[Message], list[Service]]:
        package = ""
        messages: list[Message] = []
        services: list[Service] = []

        if self.peek() == "package":
            self.pop("package")
            package_parts = [self.pop()]
            while self.peek() == ".":
                self.pop(".")
                package_parts.append(self.pop())
            self.pop(";")
            package = ".".join(package_parts)

        while self.peek() is not None:
            token = self.peek()
            if token == "message":
                messages.append(self.parse_message())
            elif token == "service":
                services.append(self.parse_service())
            else:
                raise ValueError(f"unexpected token '{token}'")
        return package, messages, services

    def parse_message(self) -> Message:
        self.pop("message")
        name = self.pop()
        self.pop("{")
        fields: list[Field] = []
        while self.peek() != "}":
            field_type = self.pop()
            name_token = self.pop()
            self.pop("=")
            tag = int(self.pop())
            self.pop(";")
            fields.append(Field(field_type, name_token, tag))
        self.pop("}")
        return Message(name, fields)

    def parse_service(self) -> Service:
        self.pop("service")
        name = self.pop()
        self.pop("{")
        methods: list[RpcMethod] = []
        while self.peek() != "}":
            self.pop("rpc")
            method_name = self.pop()
            self.pop("(")
            request_type = self.pop()
            self.pop(")")
            self.pop("returns")
            self.pop("(")
            response_type = self.pop()
            self.pop(")")
            self.pop(";")
            methods.append(RpcMethod(method_name, request_type, response_type))
        self.pop("}")
        return Service(name, methods)


def open_namespaces(package: str) -> str:
    if not package:
        return ""
    return "\n".join([f"namespace {part} {{" for part in package.split(".")])


def close_namespaces(package: str) -> str:
    if not package:
        return ""
    return "\n".join([f"}}  // namespace {part}" for part in reversed(package.split("."))])


def qualify(package: str, name: str) -> str:
    if not package:
        return name
    return "::".join(package.split(".") + [name])


def render_field_decl(field: Field) -> str:
    cpp_type, _, _, default_value = TYPE_MAP[field.field_type]
    return f"  {cpp_type} {field.name} = {default_value};"


def render_serialize(field: Field) -> str:
    _, writer_method, _, _ = TYPE_MAP[field.field_type]
    if field.field_type == "string":
      return f"  writer.{writer_method}({field.name});"
    return f"  writer.{writer_method}({field.name});"


def render_deserialize(field: Field) -> str:
    _, _, reader_method, _ = TYPE_MAP[field.field_type]
    return f"      !reader.{reader_method}(&{field.name})"


def generate_header(package: str, messages: list[Message], services: list[Service]) -> str:
    parts = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string>",
        "#include <string_view>",
        "",
        '#include "minirpc/buffer.h"',
        '#include "minirpc/channel.h"',
        '#include "minirpc/server.h"',
        '#include "minirpc/status.h"',
        "",
    ]
    ns_open = open_namespaces(package)
    if ns_open:
        parts.append(ns_open)
        parts.append("")

    for message in messages:
        parts.append(f"struct {message.name} {{")
        parts.extend(render_field_decl(field) for field in message.fields)
        if message.fields:
            parts.append("")
        parts.append("  bool SerializeToString(std::string* out) const;")
        parts.append("  bool DeserializeFromString(std::string_view data);")
        parts.append("};")
        parts.append("")

    for service in services:
        parts.append(f"class {service.name} {{")
        parts.append(" public:")
        parts.append(f"  virtual ~{service.name}() = default;")
        for method in service.methods:
            parts.append(
                f"  virtual mrpc::RpcStatus {method.name}(const {method.request_type}& request, "
                f"{method.response_type}* response) = 0;"
            )
        parts.append("};")
        parts.append("")
        parts.append(
            f"mrpc::RpcStatus Register{service.name}(mrpc::RpcServer* server, {service.name}* service);"
        )
        parts.append("")
        parts.append(f"class {service.name}Client {{")
        parts.append(" public:")
        parts.append(f"  explicit {service.name}Client(mrpc::RpcChannel* channel) : channel_(channel) {{}}")
        for method in service.methods:
            parts.append(
                f"  mrpc::RpcStatus {method.name}(const {method.request_type}& request, "
                f"{method.response_type}* response, std::uint32_t timeout_ms = 1000);"
            )
        parts.append("")
        parts.append(" private:")
        parts.append("  mrpc::RpcChannel* channel_;")
        parts.append("};")
        parts.append("")

    ns_close = close_namespaces(package)
    if ns_close:
        parts.append(ns_close)
    return "\n".join(parts) + "\n"


def generate_source(header_name: str,
                    package: str,
                    messages: list[Message],
                    services: list[Service]) -> str:
    parts = [
        f'#include "{header_name}"',
        "",
    ]

    ns_open = open_namespaces(package)
    if ns_open:
        parts.append(ns_open)
        parts.append("")

    for message in messages:
        parts.append(f"bool {message.name}::SerializeToString(std::string* out) const {{")
        parts.append("  if (out == nullptr) {")
        parts.append("    return false;")
        parts.append("  }")
        parts.append("  mrpc::BufferWriter writer;")
        for field in message.fields:
            parts.append(render_serialize(field))
        parts.append("  *out = writer.Take();")
        parts.append("  return true;")
        parts.append("}")
        parts.append("")
        parts.append(f"bool {message.name}::DeserializeFromString(std::string_view data) {{")
        parts.append("  mrpc::BufferReader reader(data);")
        if message.fields:
            conditions = " ||\n".join(render_deserialize(field) for field in message.fields)
            parts.append("  if (" + conditions + " ||\n      !reader.empty()) {")
            parts.append("    return false;")
            parts.append("  }")
        else:
            parts.append("  if (!reader.empty()) {")
            parts.append("    return false;")
            parts.append("  }")
        parts.append("  return true;")
        parts.append("}")
        parts.append("")

    for service in services:
        parts.append(
            f"mrpc::RpcStatus Register{service.name}(mrpc::RpcServer* server, {service.name}* service) {{"
        )
        parts.append("  if (server == nullptr || service == nullptr) {")
        parts.append(
            '    return mrpc::RpcStatus::Error(mrpc::StatusCode::kInternalError, "service registration failed");'
        )
        parts.append("  }")
        for method in service.methods:
            parts.append(f'  server->RegisterMethod("{service.name}", "{method.name}",')
            parts.append("      [service](const std::string& request_payload, std::string* response_payload)")
            parts.append("          -> mrpc::RpcStatus {")
            parts.append(f"        {method.request_type} request;")
            parts.append(f"        {method.response_type} response;")
            parts.append("        if (!request.DeserializeFromString(request_payload)) {")
            parts.append(
                '          return mrpc::RpcStatus::Error(mrpc::StatusCode::kBadRequest, "failed to parse request");'
            )
            parts.append("        }")
            parts.append(f"        auto status = service->{method.name}(request, &response);")
            parts.append("        if (!status.ok()) {")
            parts.append("          return status;")
            parts.append("        }")
            parts.append("        if (!response.SerializeToString(response_payload)) {")
            parts.append(
                '          return mrpc::RpcStatus::Error(mrpc::StatusCode::kSerializeError, "failed to serialize response");'
            )
            parts.append("        }")
            parts.append("        return mrpc::RpcStatus::Ok();")
            parts.append("      });")
        parts.append("  return mrpc::RpcStatus::Ok();")
        parts.append("}")
        parts.append("")

        for method in service.methods:
            parts.append(
                f"mrpc::RpcStatus {service.name}Client::{method.name}(const {method.request_type}& request, "
                f"{method.response_type}* response, std::uint32_t timeout_ms) {{"
            )
            parts.append("  if (channel_ == nullptr || response == nullptr) {")
            parts.append(
                '    return mrpc::RpcStatus::Error(mrpc::StatusCode::kInternalError, "client stub is not initialized");'
            )
            parts.append("  }")
            parts.append("  std::string request_payload;")
            parts.append("  if (!request.SerializeToString(&request_payload)) {")
            parts.append(
                '    return mrpc::RpcStatus::Error(mrpc::StatusCode::kSerializeError, "failed to serialize request");'
            )
            parts.append("  }")
            parts.append("  std::string response_payload;")
            parts.append(
                f'  auto status = channel_->Call("{service.name}", "{method.name}", request_payload, &response_payload, timeout_ms);'
            )
            parts.append("  if (!status.ok()) {")
            parts.append("    return status;")
            parts.append("  }")
            parts.append("  if (!response->DeserializeFromString(response_payload)) {")
            parts.append(
                '    return mrpc::RpcStatus::Error(mrpc::StatusCode::kSerializeError, "failed to parse response");'
            )
            parts.append("  }")
            parts.append("  return mrpc::RpcStatus::Ok();")
            parts.append("}")
            parts.append("")

    ns_close = close_namespaces(package)
    if ns_close:
        parts.append(ns_close)
    return "\n".join(parts) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--header", required=True)
    parser.add_argument("--source", required=True)
    args = parser.parse_args()

    input_path = pathlib.Path(args.input)
    header_path = pathlib.Path(args.header)
    source_path = pathlib.Path(args.source)

    package, messages, services = Parser(input_path.read_text()).parse()
    header_path.write_text(generate_header(package, messages, services))
    source_path.write_text(generate_source(header_path.name, package, messages, services))


if __name__ == "__main__":
    main()
