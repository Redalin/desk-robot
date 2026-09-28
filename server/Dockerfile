FROM python:3.13-slim

# System dependencies:
# - espeak-ng: Linux fallback voice when Fish Audio is not configured
# - libportaudio2: runtime library required by sounddevice
# - libglib2.0-0, libgomp1: required by OpenCV and ONNX runtime
# - curl, ca-certificates: model downloading and HTTPS API requests
RUN apt-get update && apt-get install -y --no-install-recommends \
    espeak-ng \
    libportaudio2 \
    libglib2.0-0 \
    libgomp1 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Install Python requirements
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application source and pre-bundled models (e.g. YuNet face detector)
COPY brain/ ./brain/
COPY models/ ./models/

# Environmental defaults
ENV PYTHONUNBUFFERED=1 \
    LIVE_VIEW_BIND=0.0.0.0 \
    LIVE_VIEW_PORT=8766 \
    DESK_ROBOT_CACHE=/root/.cache/desk-robot

EXPOSE 8765 8766

# Run brain as module
CMD ["python", "-m", "brain.main"]