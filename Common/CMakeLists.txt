cmake_minimum_required(VERSION 3.19)
project(CommonRpc LANGUAGES C)

set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ServiceRpc.idl)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Создаём папку для результатов
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Команда генерации (полностью по вашему образцу)
add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/ServiceRpc.h
        ${GENERATED_DIR}/ServiceRpc_c.c
        ${GENERATED_DIR}/ServiceRpc_s.c
    COMMAND midl
        /app_config
        /out ${GENERATED_DIR}
        /header ServiceRpc.h
        /client stub
        /cstub ServiceRpc_c.c
        /server stub
        /sstub ServiceRpc_s.c
        ${IDL_FILE}
    DEPENDS ${IDL_FILE}
    COMMENT "Generating RPC stubs (client+server) from ${IDL_FILE}"
)

# Статическая библиотека: клиентские + серверные заглушки
add_library(CommonRpc STATIC
    ${GENERATED_DIR}/ServiceRpc_c.c
    ${GENERATED_DIR}/ServiceRpc_s.c
)

# Экспортируем заголовки для пользователей библиотеки
target_include_directories(CommonRpc PUBLIC ${GENERATED_DIR})

# Обеспечиваем запуск генерации до компиляции
add_custom_target(GenerateRpc DEPENDS ${GENERATED_DIR}/ServiceRpc.h)
add_dependencies(CommonRpc GenerateRpc)
