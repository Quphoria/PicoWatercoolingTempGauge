# Font generation

set(Pix32_Font_PATH ${CMAKE_CURRENT_LIST_DIR}/Pix32_Font.png)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_fonts)
set(Pix32_Font_H_PATH ${GEN_DIR}/Pix32_Font.h)
set(Pix32_Inv_Font_H_PATH ${GEN_DIR}/Pix32_Inv_Font.h)

add_library(fonts INTERFACE)
target_include_directories(fonts INTERFACE ${GEN_DIR})

find_package(Python3)

IF (CMAKE_HOST_WIN32)
set(VENV_BIN_DIR Scripts)
ELSE()
set(VENV_BIN_DIR bin)
ENDIF()

add_custom_command(
    OUTPUT font_venv/ready # This must be a file
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/requirements.txt
    COMMAND ${Python3_EXECUTABLE} -m venv font_venv
    COMMAND ./font_venv/${VENV_BIN_DIR}/pip install -r "${CMAKE_CURRENT_LIST_DIR}/requirements.txt"
    COMMAND echo "ready" > ./font_venv/ready # signify the install was a success
)
add_custom_target(FONT_VENV DEPENDS font_venv/ready)


add_custom_command(
    OUTPUT ${Pix32_Font_H_PATH}
    DEPENDS ${Pix32_Font_PATH} font_venv/ready FONT_VENV # target level dependency to avoid race condition
    COMMAND ./font_venv/${VENV_BIN_DIR}/python "${CMAKE_CURRENT_LIST_DIR}/font_to_header.py"
            -W 6 -H 12
            "${Pix32_Font_PATH}"
            -o "${Pix32_Font_H_PATH}"
            Pix32_Font
)
add_custom_target(Pix32_Font DEPENDS ${Pix32_Font_H_PATH})
add_dependencies(fonts Pix32_Font)

add_custom_command(
    OUTPUT ${Pix32_Inv_Font_H_PATH}
    DEPENDS ${Pix32_Font_PATH} font_venv/ready FONT_VENV # target level dependency to avoid race condition
    COMMAND ./font_venv/${VENV_BIN_DIR}/python "${CMAKE_CURRENT_LIST_DIR}/font_to_header.py"
            -W 6 -H 12 -I
            "${Pix32_Font_PATH}"
            -o "${Pix32_Inv_Font_H_PATH}"
            Pix32_Inv_Font
)
add_custom_target(Pix32_Inv_Font DEPENDS ${Pix32_Inv_Font_H_PATH})
add_dependencies(fonts Pix32_Inv_Font)
