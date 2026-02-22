import requests
import json

base_url = "http://localhost:8080"

# Register model
print("Registering model...")
r = requests.post(f"{base_url}/api/models/register", json={"path": "/Users/conorrybacki/.models/Llama-3-8B-Instruct-Coder-v2-Q8_0.gguf", "display_name": ""})
model_id = r.json()["model"]["id"]

# Select model
print(f"Selecting model {model_id}...")
requests.post(f"{base_url}/api/models/select", json={"model_id": model_id})

# Send message
print("Sending chat...")
r = requests.post(f"{base_url}/api/chat/complete", json={"message": "My arbitrary favorite color is neon magenta."})
print(r.json())

# Reset
print("Resetting chat...")
requests.post(f"{base_url}/api/chat/reset")

# Ask question
print("Asking question...")
r = requests.post(f"{base_url}/api/chat/complete", json={"message": "What is my favorite color?"})
print(r.json())
