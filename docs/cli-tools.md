# MCP CLI Tools

Command-line tools for interacting with MCP (Model Context Protocol) servers.

## Overview

The mcp-glib project includes five CLI tools for inspecting and interacting with MCP servers:

| Tool | Purpose |
|------|---------|
| `mcp-inspect` | Display server info, capabilities, tools, resources, prompts |
| `mcp-call` | Call a specific tool with JSON arguments |
| `mcp-read` | Read a resource by URI |
| `mcp-prompt` | Get a prompt with arguments |
| `mcp-shell` | Interactive REPL for exploring servers |

All tools support connecting to MCP servers via stdio (subprocess), HTTP, or WebSocket transports.

## Installation

### Build Requirements

```bash
# Fedora
sudo dnf install gcc make pkgconfig glib2-devel json-glib-devel \
    libsoup3-devel libdex-devel readline-devel

# Ubuntu/Debian
sudo apt install gcc make pkg-config libglib2.0-dev libjson-glib-dev \
    libsoup-3.0-dev libdex-dev libreadline-dev
```

### Building

```bash
# Build library and tools
make all tools

# Install to /usr/local
sudo make install
```

The `mcp-shell` interactive REPL requires readline. If readline-devel is not installed, the other four tools will still build successfully.

## Common Options

All tools share these options for transport selection and output control:

| Option | Short | Description |
|--------|-------|-------------|
| `--stdio COMMAND` | `-s` | Connect via stdio to subprocess |
| `--http URL` | `-H` | Connect via HTTP (POST + SSE) |
| `--ws URL` | `-W` | Connect via WebSocket |
| `--token TOKEN` | `-t` | Bearer token for authentication |
| `--timeout SECONDS` | `-T` | Connection/request timeout (default: 30) |
| `--json` | `-j` | Output in JSON format |
| `--quiet` | `-q` | Suppress non-essential output |
| `--license` | | Show license information (AGPLv3) |
| `--help` | `-h` | Show help with examples |

**Note:** Exactly one transport option (`--stdio`, `--http`, or `--ws`) must be specified.

## Tool Reference

### mcp-inspect

Display server capabilities, tools, resources, and prompts.

**Usage:**
```bash
mcp-inspect [OPTIONS]
```

**Examples:**
```bash
# Inspect a local server via stdio
mcp-inspect --stdio ./my-server

# Inspect a remote HTTP server with authentication
mcp-inspect --http https://api.example.com/mcp --token "secret"

# Get JSON output for scripting
mcp-inspect -s ./server --json

# Inspect WebSocket server
mcp-inspect --ws wss://api.example.com/mcp
```

**Output Sections:**
- **Server:** Name, version, protocol version
- **Instructions:** Server-provided guidance (if any)
- **Tools:** Available tools with descriptions
- **Resources:** Static resources with URIs
- **Resource Templates:** Dynamic resource patterns
- **Prompts:** Available prompts with arguments

---

### mcp-call

Call a specific tool with JSON arguments.

**Usage:**
```bash
mcp-call [OPTIONS] -- TOOL_NAME 'JSON_ARGS'
```

The tool name and JSON arguments are specified after `--`.

**Examples:**
```bash
# Call an echo tool
mcp-call --stdio ./server -- echo '{"message": "hello"}'

# Call a calculator tool
mcp-call -s ./calculator-server -- add '{"a": 5, "b": 3}'

# Call with no arguments
mcp-call --stdio ./server -- get_status '{}'

# Quiet mode for scripting (only tool output)
mcp-call -q -s ./server -- sqrt '{"value": 16}'

# JSON output
mcp-call -s ./server --json -- multiply '{"x": 7, "y": 6}'
```

**Exit Codes:**

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Connection/transport error |
| 2 | Tool not found |
| 3 | Tool returned error result |

---

### mcp-read

Read a resource from an MCP server by URI.

**Usage:**
```bash
mcp-read [OPTIONS] -- URI
```

**Additional Options:**

| Option | Short | Description |
|--------|-------|-------------|
| `--output FILE` | `-o` | Write content to FILE (for binary data) |

**Examples:**
```bash
# Read a file resource
mcp-read --stdio ./server -- 'file:///etc/hostname'

# Read a configuration resource
mcp-read --http https://api.example.com/mcp -- 'config://app/settings'

# Save binary content to a file
mcp-read -s ./fs-server --output data.bin -- 'binary://file'

# Get JSON output
mcp-read -s ./server --json -- 'file:///readme'
```

**Output:**
- Text content is printed to stdout
- Binary content shows `[Binary content - use --output to save to file]`
- Multiple contents are separated with `--- URI (mime-type) ---` headers

---

### mcp-prompt

Get a prompt from an MCP server with optional arguments.

**Usage:**
```bash
mcp-prompt [OPTIONS] -- PROMPT_NAME [key=value ...]
```

Arguments are specified as `key=value` pairs after the prompt name.

**Examples:**
```bash
# Get a greeting prompt with name argument
mcp-prompt --stdio ./server -- greeting name=Alice

# Get a code review prompt with multiple arguments
mcp-prompt -H https://api.example.com/mcp -- code-review file=main.c lang=c

# Get prompt in JSON format
mcp-prompt -s ./server --json -- summarize content="some text"

# Get prompt with no arguments
mcp-prompt --stdio ./server -- help
```

**Output Format (human-readable):**
```
[user] Hello, how can I help you today?
[assistant] I'll review the code and provide feedback...
```

**Output Format (JSON):**
```json
{
  "description": "A helpful greeting",
  "messages": [
    {
      "role": "user",
      "content": [{"type": "text", "text": "Hello..."}]
    }
  ]
}
```

---

### mcp-shell

Interactive REPL for exploring MCP servers.

**Usage:**
```bash
mcp-shell [OPTIONS]
```

**Note:** Requires readline library. If readline-devel is not installed during build, mcp-shell will not be available.

**Examples:**
```bash
# Connect to a local server
mcp-shell --stdio ./server

# Connect to HTTP server with authentication
mcp-shell --http https://api.example.com/mcp --token "secret"
```

**Shell Commands:**

| Command | Description |
|---------|-------------|
| `help` | Show help message |
| `info` | Show server information |
| `tools` | List available tools |
| `resources` | List available resources |
| `templates` | List resource templates |
| `prompts` | List available prompts |
| `call <tool> <json>` | Call a tool with JSON arguments |
| `read <uri>` | Read a resource by URI |
| `get <prompt> [args]` | Get a prompt (args: key=value) |
| `quit` or `exit` | Exit the shell |

**Interactive Session Example:**
```
$ mcp-shell --stdio ./calculator-server
Connecting...
Connected to: calculator-server v1.0.0 (protocol 2025-03-26)

mcp> tools
  add - Add two numbers
  subtract - Subtract two numbers
  multiply - Multiply two numbers
  divide - Divide two numbers

mcp> call add {"a": 5, "b": 3}
8

mcp> call sqrt {"value": 16}
4

mcp> quit
Goodbye!
```

**Features:**
- Command history (in-memory, use up/down arrows)
- Line editing (emacs/vi mode based on readline configuration)
- Tab completion for commands

## Exit Codes

All tools use consistent exit codes:

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `MCP_CLI_EXIT_SUCCESS` | Operation completed successfully |
| 1 | `MCP_CLI_EXIT_ERROR` | General error (connection, transport, etc.) |
| 2 | `MCP_CLI_EXIT_NOT_FOUND` | Tool, resource, or prompt not found |
| 3 | `MCP_CLI_EXIT_TOOL_ERROR` | Tool returned an error result |

## Examples

### Scripting with JSON Output

```bash
# Get list of tools as JSON and process with jq
mcp-inspect -s ./server --json | jq '.tools[].name'

# Call a tool and extract result
result=$(mcp-call -q -s ./calculator -- add '{"a": 10, "b": 20}')
echo "Result: $result"

# Read multiple resources
for uri in "file:///a" "file:///b" "file:///c"; do
    mcp-read -q -s ./server -- "$uri"
done
```

### Testing MCP Servers

```bash
# Quick server inspection
mcp-inspect -s './my-server --debug'

# Verify a tool works
mcp-call -s ./server -- echo '{"msg": "test"}' && echo "Tool works!"

# Check resource availability
mcp-read -s ./server -- 'config://settings' || echo "Resource not found"
```

### Working with Remote Servers

```bash
# HTTP server with bearer token
export MCP_TOKEN="your-api-key"
mcp-inspect --http https://api.example.com/mcp --token "$MCP_TOKEN"

# WebSocket server
mcp-shell --ws wss://api.example.com/mcp --token "$MCP_TOKEN"
```

## Troubleshooting

### Common Issues

**"No transport specified"**
- You must provide one of `--stdio`, `--http`, or `--ws`

**"Multiple transports specified"**
- Only one transport option can be used at a time

**Connection timeout**
- Increase timeout with `--timeout` (e.g., `--timeout 60`)
- Check server is running and accessible

**"mcp-shell: command not found"**
- readline-devel was not installed during build
- Install readline-devel and rebuild with `make tools`

**GLib-GIO-CRITICAL warnings during disconnect**
- These warnings are cosmetic and don't affect functionality
- They occur during subprocess cleanup timing

## See Also

- [Server Guide](server-guide.md) - Building MCP servers with mcp-glib
- [Client Guide](client-guide.md) - Building MCP clients with mcp-glib
- [API Reference](api-reference.md) - Complete API documentation
