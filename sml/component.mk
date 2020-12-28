# Component makefile for sml

# Use libsml without UUID lib
EXTRA_CFLAGS += -DSML_NO_UUID_LIB

INC_DIRS += $(sml_ROOT) 

# args for passing into compile rule generation
sml_INC_DIR = $(sml_ROOT)libsml/sml/include
sml_SRC_DIR = $(sml_ROOT) $(sml_ROOT)libsml/sml/src

$(eval $(call component_compile_rules,sml))

