#!/usr/bin/env python3
import sys, os, subprocess, platform, argparse, json, re, shutil, fileinput, zipfile, fnmatch, glob, traceback
from os import path

REPOSITORY = path.dirname(path.abspath(__file__))
os.chdir(REPOSITORY)

# **************************************************************************************************
# Utility functions and definitions:

with open("CMakeLists.txt", "r") as f:
	cmakelists = f.read()

	match = re.search("^project\(\s*(\w+)", cmakelists, re.MULTILINE)
	if not (match and isinstance(match.group(1), str)):
		raise Exception("Couldn't find project() statement in CMakeLists.txt")
	PROJECT_NAME = match.group(1)

	# NOTE: Don't use TARGET_NAME unless you know it's required. Our CMakeLists includes this:
	# set_target_properties(Main PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")
	# So PROJECT_NAME should be used when looking for an executable.
	match = re.search("add_executable\(\s*(\w+)", cmakelists)
	if not (match and isinstance(match.group(1), str)):
		raise Exception("Couldn't find add_executable() statement in CMakeLists.txt")
	TARGET_NAME = match.group(1)

try:
	import winreg
	with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Console") as console:
		enable_ansi_colors = (winreg.QueryValue(console, "ForceV2") == 1)
except:
	enable_ansi_colors = sys.stdin.isatty()

if enable_ansi_colors:
	def info(fmt, *args): print("\033[92m", fmt.format(*args) if args else fmt, "\033[0m", sep="")
	def warn(fmt, *args): print("\033[95m", fmt.format(*args) if args else fmt, "\033[0m", sep="")
else:
	def info(fmt, *args): print(fmt.format(*args) if args else fmt)
	def warn(fmt, *args): print(fmt.format(*args) if args else fmt)

if platform.machine() in ["AMD64", "EM64T", "x64", "x86_64"]:
	HOST_ARCH = "x64"
elif platform.machine() in ["x86", "i686", "i386"]:
	HOST_ARCH = "x86"
elif platform.machine() in ["aarch64", "aarch64_be"]:
	HOST_ARCH = "arm64"
else:
	raise NotImplementedError(f"Unknown CPU architecture {platform.machine()}")

def subprocess_array_to_string(cmd):
	return " ".join([("'{}'".format(arg) if " " in arg else arg) for arg in cmd])

# **************************************************************************************************
# Parse command-line arguments:

argp = argparse.ArgumentParser(description=f"Builds and launches {PROJECT_NAME}",
	epilog=f"Unknown arguments will be passed to {PROJECT_NAME} if run")

arg_b = argp.add_argument_group("build configuration")
arg_b.add_argument("-c", "--clean", action="store_true", help="Clean build directory before starting the build")
arg_b.add_argument("-C", "--cmake", action="store_true", help="Re-run CMake before starting the build")
arg_b.add_argument("-R", "--release", action="store_true", help="Build in release mode")
arg_b.add_argument("-G", "--generator", help="Specify a CMake generator to use")
arg_b.add_argument("-A", "--arch", choices=["x64", "x86", "arm64"], default=HOST_ARCH, help="CPU architecture to build for")
arg_b.add_argument("-D", action="append", nargs="?", metavar="VAR=VALUE", help="Specify additional CMake variables")

arg_p = argp.add_argument_group("platform to build for").add_mutually_exclusive_group()
arg_p.add_argument("--web", action="store_true", help="Build for the web platform using Emscripten")
arg_p.add_argument("--ios", action="store_true", help="Build for iOS (not supported yet)")
arg_p.add_argument("--android", action="store_true", help="Build for Android (not supported yet)")

arg_a = argp.add_argument_group("after-build actions").add_mutually_exclusive_group()
arg_a.add_argument("-r", "--run", action="store_true", help="Launch the game after building it")
arg_a.add_argument("-i", "--ide", action="store_true", help="Generate and open an IDE project")
arg_a.add_argument("--renderdoc", action="store_true", help="Debug the game using RenderDoc")
arg_a.add_argument("--package", action="store_true", help="Package game for distribution (web only for now)")

args, EXTRA_RUN_ARGS = argp.parse_known_args()

TARGET_ARCH = args.arch if args.arch else HOST_ARCH
VSDEVCMD_HOST_ARCH   = {"x64": "amd64", "x86": "x86", "arm64": "arm64"}[HOST_ARCH]
VSDEVCMD_TARGET_ARCH = {"x64": "amd64", "x86": "x86", "arm64": "arm64"}[TARGET_ARCH]
CMAKE_HOST_ARCH      = {"x64": "x64", "x86": "Win32", "arm64": "ARM64"}[HOST_ARCH]
CMAKE_TARGET_ARCH    = {"x64": "x64", "x86": "Win32", "arm64": "ARM64"}[TARGET_ARCH]

EXTRA_BUILD_VARS = {}
if args.D:
	for dvar in args.D:
		var = dvar.split("=", 1)[0]
		val = dvar.split("=", 1)[1]
		assert var not in EXTRA_BUILD_VARS
		EXTRA_BUILD_VARS[var] = val

CLEAN_BUILD_DIRECTORY = args.clean
FORCE_RERUN_CMAKE = args.cmake
REQUESTED_GENERATOR = args.generator

PLATFORM = "native"
if args.web: PLATFORM = "web"
if args.ios: PLATFORM = "ios"
if args.android: PLATFORM = "android"
CMAKE_TOOLCHAIN = None

if (args.release or args.package) and PLATFORM == "web":
	# Emscripten limits WASM-level (binaryen) optimisations in RelWithDebInfo mode
	BUILD_CONFIGURATION = "Release"
elif (args.release or args.package):
	BUILD_CONFIGURATION = "RelWithDebInfo"
else:
	BUILD_CONFIGURATION = "Debug"

# TODO: Supporting mobile platforms will involve lots of changes
if args.ios:
	raise NotImplementedError("Building for iOS is not supported")
if args.android:
	raise NotImplementedError("Building for Android is not supported")

LAUNCH_GAME = args.run
LAUNCH_IDE = args.ide
LAUNCH_RENDERDOC = args.renderdoc
PACKAGE_GAME = args.package

# **************************************************************************************************
# Import Visual Studio environment using VsDevCmd if on Windows:

def import_visual_studio_env():
	# Try to load environment from cache (VsDevCmd is fairly slow):
	cache = {}
	cache_key = f"{HOST_ARCH}-{TARGET_ARCH}"
	cache_filename = path.join("cache", "msvc_environment.json")
	os.makedirs(path.dirname(cache_filename), exist_ok=True)
	try:
		with open(cache_filename, "r") as f:
			cache = json.load(f)
			if cache[cache_key]["original_path"] == os.getenv("PATH"):
				assert len(cache[cache_key]["new_environment"]) != 0
				for name, value in cache[cache_key]["new_environment"].items():
					os.environ[name] = value
				return
	except:
		cache[cache_key] = {"original_path": os.getenv("PATH"), "new_environment": {}}

	# Find installation path:
	try:
		min_version = 15 # Visual Studio 2017
		vspath = subprocess.check_output(["external/vswhere.exe", "-version", f'[{min_version},)',
			"-latest", "-prerelease", "-utf8", "-property", "installationPath"],
			shell=False, universal_newlines=True).strip()
	except Exception as e:
		warn("Skipping Visual Studio environment setup as no usable version could be found.")

	# Retrieve environment variables:
	# See https://github.com/microsoft/vswhere/wiki/Start-Developer-Command-Prompt, "Using PowerShell"
	try:
		vsdevcmd = path.join(vspath.strip(), "Common7/Tools/vsdevcmd.bat")
		cmd = [vsdevcmd, "-no_logo", f"-host_arch={VSDEVCMD_HOST_ARCH}", f"-arch={VSDEVCMD_TARGET_ARCH}", "&&", "set"]
		p = subprocess.Popen(cmd, shell=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		out, err = p.communicate()
		if p.returncode != 0:
			raise Exception(f"VsDevCmd exited with code {p.returncode}", out, err)
	except Exception as e:
		warn("Failed to load Visual Studio environment:")
		traceback.print_exc()

	# Set new environment variables and write them into the cache:
	for line in out.splitlines():
		try:
			name, value = line.split("=", 1)
			if os.getenv(name) != value:
				os.environ[name] = value
				cache[cache_key]["new_environment"][name] = value
		except:
			pass
	with open(cache_filename, "w") as f:
		json.dump(cache, f, indent=4)

	if shutil.which("cl"):
		info("Loaded Visual Studio environment from VsDevCmd (will be cached)")
	else:
		warn("Imported Visual Studio environment but MSVC is not available.")
		warn("Make sure you've installed the appropriate C++ development workloads.")

if platform.system() == "Windows":
	import_visual_studio_env()

# **************************************************************************************************
# Fetch external dependencies:

info("Fetching external dependencies (if needed)")

EXTERNAL_DIR=path.join(REPOSITORY, "external")
EXTERNAL_CMAKE_DIR=path.join("cache", "external_fetch")
os.makedirs(EXTERNAL_CMAKE_DIR, exist_ok=True)

try:
	subprocess.check_call(["cmake", "--build", "."], cwd=EXTERNAL_CMAKE_DIR)
except:
	try:
		cmd = ["cmake"] + (["-G", "Ninja"] if shutil.which("ninja") else []) + [EXTERNAL_DIR]
		subprocess.check_call(cmd, cwd=EXTERNAL_CMAKE_DIR)
		subprocess.check_call(["cmake", "--build", "."], cwd=EXTERNAL_CMAKE_DIR)
	except:
		warn("Failed to retrieve external dependencies.")
		warn("Try deleting any failed dependencies from external/.")
		exit(1)

# **************************************************************************************************
# Fetch and configure Emscripten SDK if building for the web platform:

if PLATFORM == "web":
	# Emscripten releases: https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md
	EMSDK_VER = "3.1.46" # from 2023-09-15
	info(f"Setting up Emscripten {EMSDK_VER}")

	EMSDK_DIR=path.join(EXTERNAL_DIR, "emsdk")
	if platform.system() == "Windows": EMSDK=path.join(EMSDK_DIR, "emsdk.bat")
	else: EMSDK=path.join(EMSDK_DIR, "emsdk")

	subprocess.check_call([EMSDK, "install", EMSDK_VER], cwd=EMSDK_DIR)
	subprocess.check_call([EMSDK, "activate", EMSDK_VER], cwd=EMSDK_DIR)

	CMAKE_TOOLCHAIN = path.join(EMSDK_DIR, "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

# **************************************************************************************************
# Select CMake generator and make up a name for the build directory:

# Determine default CMake generator:
DEFAULT_GENERATOR = None
FIRST_VISUAL_STUDIO_GENERATOR = None
p = subprocess.Popen(["cmake", "-G"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
out, err = p.communicate()
for line in out.splitlines():
	match = re.match(r'^\* ([^=]+)', line)
	if match:
		DEFAULT_GENERATOR = match.group(1).strip()
	match = re.match(r'Visual Studio [0-9]+ [0-9]+', line)
	if match:
		FIRST_VISUAL_STUDIO_GENERATOR = match.group(0)
if DEFAULT_GENERATOR is None:
	warn("Failed to determine default CMake generator. Will let CMake select one.")

if REQUESTED_GENERATOR:
	GENERATOR = REQUESTED_GENERATOR
else:
	# If the user asked for an IDE to be launched, we need to pick an appropriate generator:
	if LAUNCH_IDE and platform.system() == "Windows":
		if DEFAULT_GENERATOR and DEFAULT_GENERATOR.startswith("Visual Studio"):
			GENERATOR = DEFAULT_GENERATOR
		elif FIRST_VISUAL_STUDIO_GENERATOR:
			GENERATOR = FIRST_VISUAL_STUDIO_GENERATOR
		else:
			raise Exception("Your version of CMake does not support Visual Studio project generation")
	elif LAUNCH_IDE and platform.system() == "Darwin":
		GENERATOR = "Xcode"
	# Prefer Ninja if it's available and there are no constraints:
	elif shutil.which("ninja"):
		GENERATOR = "Ninja"
	elif DEFAULT_GENERATOR:
		GENERATOR = DEFAULT_GENERATOR
	else:
		GENERATOR = None

if GENERATOR:
	gen_name = re.sub("Visual Studio [0-9]+ ([0-9]+)", "VS\\1", GENERATOR)
	gen_name = gen_name.replace(" - ", "").replace(" ", "")
	cfg_gen_name = f"{BUILD_CONFIGURATION}-{gen_name}"
else:
	cfg_gen_name = f"{BUILD_CONFIGURATION}"

if PLATFORM == "native":
	BUILD_NAME = f"{TARGET_ARCH}-{cfg_gen_name.lower()}"
else:
	BUILD_NAME = f"{PLATFORM}-{cfg_gen_name.lower()}"

BUILD_DIR = path.join("cache", BUILD_NAME)

if CLEAN_BUILD_DIRECTORY:
	info(f"Cleaning build directory: {BUILD_DIR}")
	shutil.rmtree(BUILD_DIR, ignore_errors=True)
else:
	info(f"Build directory: {BUILD_DIR}")

os.makedirs(BUILD_DIR, exist_ok=True)

# **************************************************************************************************
# Configure build with CMake:

cmd = ["cmake", f"-DCMAKE_BUILD_TYPE={BUILD_CONFIGURATION}"]
if GENERATOR:
	cmd.extend(["-G", GENERATOR])
if PLATFORM == "native" and not LAUNCH_IDE:
	cmd.append("-DCMAKE_EXPORT_COMPILE_COMMANDS=1")
if CMAKE_TOOLCHAIN:
	cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={CMAKE_TOOLCHAIN}")
if EXTRA_BUILD_VARS:
	cmd.extend([f"-D{k}={v}" for k, v in EXTRA_BUILD_VARS.items()])

# Cache compiler check results. Checks performed when building with one generator won't be carried
# out when building with another. This is useful since Ninja is much faster than Xcode or MSBuild.
# More details: https://github.com/cristianadam/cmake-checks-cache
cmake_checks_cache = path.realpath(path.join("cache", "cmake_checks_cache.txt"))
if path.isfile(cmake_checks_cache):
	cmd.extend(["-C", cmake_checks_cache])
else:
	cmake_checks_cache_module_dir = path.realpath("External/cmake_checks_cache/CMakeChecksCache")
	cmake_checks_cache_module_dir = cmake_checks_cache_module_dir.replace("\\", "/")
	cmd.append("-DCMAKE_MODULE_PATH='{}'".format(cmake_checks_cache_module_dir))

# Skip CMake run if we know the generator will take care of it:
if not FORCE_RERUN_CMAKE and GENERATOR == "Ninja" and path.isfile(path.join(BUILD_DIR, "build.ninja")):
	pass
else:
	cmd.append(path.relpath(REPOSITORY, BUILD_DIR))
	info("Generating build script: {}", subprocess_array_to_string(cmd))
	subprocess.check_call(cmd, cwd=path.realpath(BUILD_DIR))

# **************************************************************************************************
# Start build:

if PLATFORM == "web":
	# Always force relink since CMake can't tell when shell.html changes
	HTML_FILE = path.join(BUILD_DIR, f"{PROJECT_NAME}.html")
	if path.isfile(HTML_FILE):
		os.remove(HTML_FILE)

if not LAUNCH_IDE:
	cmd = ["cmake", "--build", ".", "--config", BUILD_CONFIGURATION]
	info("Building: {}", subprocess_array_to_string(cmd))
	subprocess.check_call(cmd, cwd=path.realpath(BUILD_DIR))

	# Copy compile_commands.json to repository root if generated.
	# Emscripten builds don't generate a valid listing, so we only want to do this on native.
	if PLATFORM == "native":
		if path.isfile(path.join(BUILD_DIR, "compile_commands.json")):
			if path.isfile("compile_commands.json"):
				os.remove("compile_commands.json")
			shutil.copy(path.join(BUILD_DIR, "compile_commands.json"), "compile_commands.json")
			info(f"Copied compile_commands.json from {BUILD_DIR} to repository root")

	# Copy compiler checks cache if generated.
	generated_checks_cache = path.join(BUILD_DIR, "cmake_checks_cache.txt")
	if path.isfile(generated_checks_cache):
		# Make sure the checks cache only includes HAVE_ variables:
		with open(cmake_checks_cache, "w") as cleaned:
			with open(generated_checks_cache, "r") as original:
				for line in original:
					if "HAVE_" in line:
						cleaned.write(line)
		info(f"Copied CMake checks cache from {BUILD_DIR} to Cache")

# **************************************************************************************************
# Run the game if the script is run with -r/--run:

if PLATFORM == "native" and not LAUNCH_IDE:
	EXECUTABLE = None
	possible_executable_paths = [
		path.join(BUILD_DIR, BUILD_CONFIGURATION, f"{PROJECT_NAME}.exe"), # Visual Studio
		path.join(BUILD_DIR, BUILD_CONFIGURATION, f"{PROJECT_NAME}"), # Xcode
		path.join(BUILD_DIR, f"{PROJECT_NAME}.exe"), # Windows
		path.join(BUILD_DIR, f"{PROJECT_NAME}"), # Unix
	]
	for p in possible_executable_paths:
		if path.isfile(p):
			EXECUTABLE = p
			break
	if not EXECUTABLE:
		raise Exception("Couldn't find game executable under {}".format(BUILD_DIR))

	if LAUNCH_GAME:
		cmd = [EXECUTABLE] + EXTRA_RUN_ARGS
		info("Launching game: {}", subprocess_array_to_string(cmd))
		try:
			ret = subprocess.call(cmd, cwd=REPOSITORY)
			if ret != 0:
				warn("Game exited with code {}".format(ret))
		except KeyboardInterrupt:
			pass

if PLATFORM == "web" and LAUNCH_GAME:
	from http.server import HTTPServer, SimpleHTTPRequestHandler
	os.chdir(BUILD_DIR)
	# TODO: Allow address/port to be specified. Increment and retry if port is unavailable.
	httpd = HTTPServer(('0.0.0.0', 8000), SimpleHTTPRequestHandler)
	if platform.system() == "Windows":
		# 0.0.0.0 doesn't work on my Windows machine for some reason.
		url = f"http://localhost:{httpd.server_address[1]}/{PROJECT_NAME}.html"
	else:
		url = f"http://{httpd.server_address[0]}:{httpd.server_address[1]}/{PROJECT_NAME}.html"
	info(f"Launching web server at {url}")
	try:
		httpd.serve_forever()
	except:
		print() # Don't mess up the shell prompt on Ctrl-C
	sys.exit(0)

# **************************************************************************************************
# Start Visual Studio or Xcode if the script is run with -i/--ide:

if LAUNCH_IDE:
	if platform.system() == "Windows":
		cmd = ["devenv.exe", path.join(BUILD_DIR, f"{PROJECT_NAME}.sln")]
		info("Launching Visual Studio: {}", subprocess_array_to_string(cmd))
		subprocess.Popen(cmd)

	elif platform.system() == "Darwin":
		# To set the working directory, we have to edit the project's scheme file.
		xcproj = path.join(BUILD_DIR, f"{PROJECT_NAME}.xcodeproj")
		# Must use TARGET_NAME when looking for the scheme.
		xcs = path.join(xcproj, "xcshareddata", "xcschemes", f"{TARGET_NAME}.xcscheme")
		edited = False
		with fileinput.input(xcs, inplace=True) as f:
			for line in f:
				# Both spacings are used. I think CMake uses the first one and Xcode the second.
				if 'useCustomWorkingDirectory="' in line or 'useCustomWorkingDirectory = "' in line:
					edited = True
					print('      useCustomWorkingDirectory="YES"')
					print('      customWorkingDirectory="{}"'.format(REPOSITORY))
				elif 'customWorkingDirectory' in line:
					pass
				else:
					print(line)
		if edited:
			info(f"Edited {xcs} to set working directory")
		else:
			warn(f"Didn't find useCustomWorkingDirectory in {xcs}")
		cmd = ["open", xcproj]
		info("Launching Xcode: {}", subprocess_array_to_string(cmd))
		subprocess.Popen(cmd)

# **************************************************************************************************
# Start RenderDoc if the script is run with --renderdoc:

if LAUNCH_RENDERDOC:
	renderdoc = None
	if shutil.which("qrenderdoc"):
		renderdoc = "qrenderdoc"
	elif platform.system() == "Windows":
		p = path.join(os.getenv("PROGRAMFILES", ""), "RenderDoc", "qrenderdoc.exe")
		if path.isfile(p):
			renderdoc = p
	if not renderdoc:
		raise Exception("Couldn't find RenderDoc. Please install it or make sure qrenderdoc is in your PATH.")

	# Capture settings, obtained by reading a default RenderDoc cap file:
	capture_settings = {
		"rdocCaptureSettings": 1,
		"settings": {
			"autoStart": False,
			"commandLine": "",
			"environment": [],
			"executable": path.realpath(EXECUTABLE),
			"inject": False,
			"options": {
				"allowFullscreen": True,
				"allowVSync": True,
				"apiValidation": True,
				"captureAllCmdLists": False,
				"captureCallstacks": False,
				"captureCallstacksOnlyDraws": False,
				"debugOutputMute": True,
				"delayForDebugger": 0,
				"hookIntoChildren": False,
				"refAllResources": False,
				"verifyBufferAccess": False,
			},
			"workingDir": REPOSITORY,
		}
	}
	capfile = path.join("cache", "renderdoc.cap")
	with open(capfile, "w") as f:
		json.dump(capture_settings, f)

	cmd = [renderdoc, capfile]
	info("Launching RenderDoc: {}", subprocess_array_to_string(cmd))
	subprocess.Popen(cmd)

# **************************************************************************************************
# Package game for distribution if script is run with --package:

if PLATFORM == "web" and PACKAGE_GAME:
	outdir = "docs" # expected by GitHub Pages
	os.makedirs(outdir, exist_ok=True)
	info(f"Packaging game: {outdir}/{PROJECT_NAME}.html")
	shutil.copyfile(path.join(BUILD_DIR, f"{PROJECT_NAME}.html"), path.join(outdir, "index.html"))
	shutil.copyfile(path.join(BUILD_DIR, f"{PROJECT_NAME}.js"),   path.join(outdir, f"{PROJECT_NAME}.js"))
	shutil.copyfile(path.join(BUILD_DIR, f"{PROJECT_NAME}.wasm"), path.join(outdir, f"{PROJECT_NAME}.wasm"))
	shutil.copyfile(path.join(BUILD_DIR, f"{PROJECT_NAME}.data"), path.join(outdir, f"{PROJECT_NAME}.data"))
	open(path.join(outdir, ".nojekyll"), "w").close() # expected by GitHub Pages
