# Use ROS 2 Humble (Stable Standard)
FROM ros:humble

# Prevent Python from buffering stdout (so you see prints immediately)
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

# Install basic tools
RUN apt-get update && apt-get install -y \
    python3-pip \
    iputils-ping \
    nano \
    && rm -rf /var/lib/apt/lists/*

# Install the critical Python libraries
# - eclipse-zenoh: The CORRECT package name for Zenoh
# - mujoco: Physics engine
# - pinocchio: Robot math library
RUN pip3 install eclipse-zenoh mujoco pinocchio numpy orjson

# Set work directory
WORKDIR /app

# Keep container alive
CMD ["tail", "-f", "/dev/null"]