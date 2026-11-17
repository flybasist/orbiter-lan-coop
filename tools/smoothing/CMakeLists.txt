# Smoothing simulation: the host sends states over UDP, the client reconstructs the motion.
add_executable(olc-smoothing
    main.cpp
    trajectory.cpp
    network.cpp
    methods.cpp
)
