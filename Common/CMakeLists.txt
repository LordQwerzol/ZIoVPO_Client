cmake_minimum_required(VERSION 3.19)
project(CommonRpc LANGUAGES C)   # явно указываем язык C

set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ServiceRpc.idl)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Команда для генерации RPC-заглушек
add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/ServiceRpc.h
        ${GENERATED_DIR}/ServiceRpc_c.c
        ${GENERATED_DIR}/ServiceRpc_s.c
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND midl /app_config /out ${GENERATED_DIR} /header ServiceRpc.h /cstub ServiceRpc_c.c /sstub ServiceRpc_s.c ${IDL_FILE}    DEPENDS ${IDL_FILE}
    COMMENT "Generating RPC stubs from IDL"
)

# Статическая библиотека из сгенерированных файлов
add_library(CommonRpc STATIC
    ${GENERATED_DIR}/ServiceRpc_c.c
    ${GENERATED_DIR}/ServiceRpc_s.c
)

# Добавляем папку с заголовками для клиентов
target_include_directories(CommonRpc PUBLIC ${GENERATED_DIR})

# Гарантируем, что генерация выполнится до компиляции библиотеки
add_custom_target(GenerateRpc DEPENDS ${GENERATED_DIR}/ServiceRpc.h)
add_dependencies(CommonRpc GenerateRpc)
