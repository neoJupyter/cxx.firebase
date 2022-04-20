cmake_minimum_required(VERSION 3.11)

project(njp.cxx)

kls_add_loadable_module(njp.proxy njpxx::proxy)
kls_module_source_directory(njp.proxy Module)
target_link_libraries(
        njp.proxy PUBLIC
        njpxx::common
        klsxx::essential klsxx::coroutine klsxx::thread klsxx::io klsxx::phttp
)
