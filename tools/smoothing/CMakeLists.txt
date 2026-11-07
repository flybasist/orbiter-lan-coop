# Симуляция сглаживания: хост шлёт состояния по UDP, клиент восстанавливает движение.
add_executable(olc-smoothing
    main.cpp
    trajectory.cpp
    network.cpp
    methods.cpp
)
