# Planning Module

This module is responsible for planning and communication between agents in a multi-agent system. It uses ROS 2 for communication and Docker for containerization.

## Prerequisites

- ROS 2 Humble
- Docker
- Docker Compose


## Building and Running with Docker Compose

1. **Install Docker**: Follow the instructions on the [Docker installation page](https://docs.docker.com/get-docker/).

2. **Install Docker Compose**: Follow the instructions on the [Docker Compose installation page](https://docs.docker.com/compose/install/).

3. **Clone the repository**:
    ```bash
    git clone https://github.com/your-repo/swarmtalk.git
    cd swarmtalk
    ```

4. **Build and run the containers**:
    ```bash
    docker-compose up --build
    ```

5. **Check the logs**:
    ```bash
    docker-compose logs -f
    ```

## Building and Running with ROS 2

1. **Install ROS 2 Humble**: Follow the instructions on the [ROS 2 installation page](https://docs.ros.org/en/humble/Installation.html).

2. **Clone the repository**:
    ```bash
    git clone https://github.com/your-repo/swarmtalk.git
    cd swarmtalk/planning
    ```

3. **Install dependencies**:
    ```bash
    sudo apt-get update
    sudo apt-get install python3-pip python3-setuptools
    pip3 install --no-cache-dir pyserial trajallocpy==0.0.14 shapely extremitypathfinder[numba] matplotlib numpy geojson
    ```

4. **Build the ROS 2 workspace**:
    ```bash
    cd /home/kang/workspace/swarmtalk/planning
    source /opt/ros/humble/setup.bash
    colcon build
    ```

5. **Run the planner node**:
    ```bash
    source /opt/ros/humble/setup.bash
    source install/setup.bash
    ros2 run planning planner --ros-args --params-file /config/config.yaml
    ```

## Configuration

The configuration files for each agent are located in the `config` directory. Modify these files to change the parameters for each agent.

## License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.