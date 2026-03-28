# Petting Zoo

A web frontend for [zoo-keeper-server](https://github.com/crybo-rybo/zoo-keeper-server) — local LLM inference in your browser.

## Prerequisites

- [Docker](https://docs.docker.com/get-docker/) and Docker Compose
- [Node.js 22+](https://nodejs.org/) (for local frontend development)
- A GGUF model file (place in `models/` directory)

## Quick Start

```bash
git clone --recursive https://github.com/crybo-rybo/petting-zoo.git
cd petting-zoo

# Place a GGUF model in models/
cp /path/to/your-model.gguf models/model.gguf

# Start everything
docker compose up --build
```

- Frontend: http://localhost:5173
- Server API: http://localhost:8080

## Local Frontend Development

If you prefer running the frontend natively (faster hot reload):

```bash
npm ci
npm run dev
```

This requires zoo-keeper-server running separately on `localhost:8080`.

## Testing

```bash
npx vitest run
```

## Project Structure

```
petting-zoo/
├── zoo-keeper-server/      # Git submodule — C++ inference server
├── docker-compose.yml      # Runs server + frontend together
├── Dockerfile.server       # Builds zoo-keeper-server from source
├── src/                    # Svelte frontend source
│   ├── App.svelte
│   ├── lib/api/            # Typed API client
│   └── routes/             # Page components
├── tests/                  # Vitest unit tests
└── models/                 # Mount point for GGUF model files
```
