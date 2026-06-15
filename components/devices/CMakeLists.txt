set(src_dirs       
            LED
            KEY
            LCD
            XL9555
            AP3216C
            QMA6100P
            SPI_SD
            ES8388
            )

set(include_dirs
            LED
            KEY
            LCD
            XL9555
            AP3216C
            QMA6100P
            SPI_SD
            ES8388
            )

set(requires
            driver
            esp_lcd
            esp_timer
            fatfs
            bus_drivers
            board
            )

idf_component_register(SRC_DIRS ${src_dirs} 
                       INCLUDE_DIRS ${include_dirs} 
                       REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)