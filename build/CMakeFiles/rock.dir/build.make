# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.3

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/wangzhi/spear

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/wangzhi/spear/build

# Include any dependencies generated for this target.
include CMakeFiles/rock.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/rock.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rock.dir/flags.make

CMakeFiles/rock.dir/ab.cc.o: CMakeFiles/rock.dir/flags.make
CMakeFiles/rock.dir/ab.cc.o: ../ab.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/wangzhi/spear/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rock.dir/ab.cc.o"
	/usr/lib64/ccache/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/rock.dir/ab.cc.o -c /home/wangzhi/spear/ab.cc

CMakeFiles/rock.dir/ab.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rock.dir/ab.cc.i"
	/usr/lib64/ccache/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/wangzhi/spear/ab.cc > CMakeFiles/rock.dir/ab.cc.i

CMakeFiles/rock.dir/ab.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rock.dir/ab.cc.s"
	/usr/lib64/ccache/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/wangzhi/spear/ab.cc -o CMakeFiles/rock.dir/ab.cc.s

CMakeFiles/rock.dir/ab.cc.o.requires:

.PHONY : CMakeFiles/rock.dir/ab.cc.o.requires

CMakeFiles/rock.dir/ab.cc.o.provides: CMakeFiles/rock.dir/ab.cc.o.requires
	$(MAKE) -f CMakeFiles/rock.dir/build.make CMakeFiles/rock.dir/ab.cc.o.provides.build
.PHONY : CMakeFiles/rock.dir/ab.cc.o.provides

CMakeFiles/rock.dir/ab.cc.o.provides.build: CMakeFiles/rock.dir/ab.cc.o


# Object files for target rock
rock_OBJECTS = \
"CMakeFiles/rock.dir/ab.cc.o"

# External object files for target rock
rock_EXTERNAL_OBJECTS =

librock.a: CMakeFiles/rock.dir/ab.cc.o
librock.a: CMakeFiles/rock.dir/build.make
librock.a: CMakeFiles/rock.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/wangzhi/spear/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library librock.a"
	$(CMAKE_COMMAND) -P CMakeFiles/rock.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rock.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rock.dir/build: librock.a

.PHONY : CMakeFiles/rock.dir/build

CMakeFiles/rock.dir/requires: CMakeFiles/rock.dir/ab.cc.o.requires

.PHONY : CMakeFiles/rock.dir/requires

CMakeFiles/rock.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rock.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rock.dir/clean

CMakeFiles/rock.dir/depend:
	cd /home/wangzhi/spear/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/wangzhi/spear /home/wangzhi/spear /home/wangzhi/spear/build /home/wangzhi/spear/build /home/wangzhi/spear/build/CMakeFiles/rock.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rock.dir/depend

