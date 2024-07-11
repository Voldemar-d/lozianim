﻿#include "lozianim.h"
#include "inputparser.h"
#include "lozidraw.h"
#include "coefanim.h"

namespace fs = std::filesystem;
namespace cr = std::chrono;

void printHelp(char** argv, const bool bFull) {
	fs::path fmain(argv[0]);
	std::cout << "Usage: " << fmain.filename() << " [options]" << '\n';
	if (!bFull) {
		std::cout << "Display help for all options:\n";
		std::cout << fmain.filename() << " -help\n";
		return;
	}
	std::cout << "\noptions can be:" << '\n';
	std::cout << "-help\t\t\tdisplay this help" << '\n';
	std::cout << "-width {N}\t\tset output image width in pixels, 1280 by default" << '\n';
	std::cout << "-height {N}\t\tset output image height in pixels, 720 by default" << '\n';
	std::cout << "-outfolder {path}\tset output folder (will be created it doesn't exist) for saving image files" << '\n';
	std::cout << "-steps {N}\t\tset number of output images (animation steps), 1 by default" << '\n';
	std::cout << "-coef1 {v}\t\tset value (float) of coefficient B, -1.0 by default" << '\n';
	std::cout << "-coef2 {v}\t\tset value (float) of coefficient C, 0.5 by default" << '\n';
	std::cout << "-coef1end {v}\t\tset ending value (float) of coefficient B, -1.0 by default" << '\n';
	std::cout << "-coef2end {v}\t\tset ending value (float) of coefficient C, 1.0 by default" << '\n';
	std::cout << "-coefin {file}\t\tget B/C coefficient animation from specified text file" << '\n';
	std::cout << "-threads {N}\t\tset number of running threads: use -threads max to use CPU cores number,\nuse -threads half to use 1/2 CPU cores number (default) or specify a number, e.g. -threads 4" << '\n';
	std::cout << std::thread::hardware_concurrency() << " maximal threads available." << '\n';
	std::cout << '\n' << "Pressing 'q' stops writing image series." << '\n';
	coefAnim::outfmt();
}

std::string getFullPath(std::string_view folder) {
	fs::path path(folder);
	if (path.has_root_path()) // already full path
		return std::string(folder);
	auto curpath = fs::absolute(path);
	return curpath.string();
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printHelp(argv, false);
		return 0;
	}
	std::vector<std::string> all_args;
	if (argc > 1)
		all_args.assign(argv + 1, argv + argc);
	InputParser input(all_args);

	if (input.cmdOptionExists("-help")) {
		printHelp(argv, true);
		return 0;
	}

	std::string outfolder;
	if (input.cmdOptionExists("-outfolder")) {
		const auto& param = input.getThisOption();
		outfolder = getFullPath(param);
	}
	else
		outfolder = getFullPath(".");
	if (!fs::exists(outfolder) && !fs::create_directory(outfolder)) {
		std::cout << "Failed to create output folder: " << outfolder << '\n';
		return -1;
	}

	int width = 1280, height = 720;
	if (input.cmdOptionExists("-width")) {
		const auto& param = input.getThisOption();
		width = atoi(param.c_str());
	}
	if (input.cmdOptionExists("-height")) {
		const auto& param = input.getThisOption();
		height = atoi(param.c_str());
	}
	if (width < 2 || height < 2) {
		std::cout << "Width and height must be not less than 2 pixels\n";
		return -1;
	}

	coefAnim coef;
	if (input.cmdOptionExists("-coefin")) {
		if (!coef.parse(input.getThisOption())) {
			std::cout << "Failed to parse coefficient animation from specified file.\n";
			return -1;
		}
	}
	const auto ntotal = coef.total();

	const int nmaxthreads = std::thread::hardware_concurrency();
	int nthreads = 0;
	if (input.cmdOptionExists("-threads")) {
		const auto& param = input.getThisOption();
		if (param == "max")
			nthreads = nmaxthreads;
		else if (param == "half")
			nthreads = nmaxthreads / 2;
		else
			nthreads = atoi(param.c_str());
	}
	if (nthreads < 1)
		nthreads = nmaxthreads / 2;
	else if (nthreads > nmaxthreads)
		nthreads = nmaxthreads;

	double coef1 = -1.0, coef2 = 0.5, coef1end = -1.0, coef2end = 1.0;
	int nsteps = 1;
	if (ntotal < 1) { // no coefficient animation file specified
		if (input.cmdOptionExists("-coef1")) {
			const auto& param = input.getThisOption();
			coef1 = atof(param.c_str());
		}
		if (input.cmdOptionExists("-coef1end")) {
			const auto& param = input.getThisOption();
			coef1end = atof(param.c_str());
		}
		if (input.cmdOptionExists("-coef2")) {
			const auto& param = input.getThisOption();
			coef2 = atof(param.c_str());
		}
		if (input.cmdOptionExists("-coef2end")) {
			const auto& param = input.getThisOption();
			coef2end = atof(param.c_str());
		}
		if (input.cmdOptionExists("-steps")) {
			const auto& param = input.getThisOption();
			nsteps = atoi(param.c_str());
		}
		if (nsteps < 1) nsteps = 1;
	}

	auto saveSteps = [width, height, &outfolder](int nstart, int ntotal, int nthreads,
		double coef1, double coef1end, double coef2, double coef2end, int nsteps)
	{
		auto funcStop = []() {
#ifdef _WIN32
			if (_kbhit() && 'q' == _getch())
				return true;
#else
			auto kbhit = []() {
				struct termios oldt, newt;
				int ch, oldf;
				tcgetattr(STDIN_FILENO, &oldt);
				newt = oldt;
				newt.c_lflag &= ~(ICANON | ECHO);
				tcsetattr(STDIN_FILENO, TCSANOW, &newt);
				oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
				fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
				ch = getchar();
				tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
				fcntl(STDIN_FILENO, F_SETFL, oldf);
				if (ch != EOF) {
					ungetc(ch, stdin);
					return std::make_pair(true, ch);
				}
				return std::make_pair(true, ch);
			};

			const auto [hit, ch] = kbhit();
			if (hit && 'q' == ch) return true;
#endif
			return false;
		};
		
		bool bBreak = false;
		const auto tstart = cr::steady_clock::now();
		{
			CFracDraw frac(nstart); int cnt = -1;
			frac.SaveSteps(nthreads, outfolder, width, height, coef1, coef1end, coef2, coef2end, nsteps);
			for (;;) {
				if (funcStop()) {
					bBreak = true;
					break;
				}
				std::this_thread::sleep_for(cr::milliseconds{ 500 });
				if (frac.Finished())
					break;
				if (frac.GetCount() != cnt) {
					cnt = frac.GetCount();
					std::cout << "Saving image: " << cnt + 1 << "/" << ntotal << '\r';
				}
			}
			if (bBreak)
				std::cout << "\nFinishing...\n";
		}
		return bBreak;
	};

	auto showStart = [width, height, &outfolder, nthreads](int nImages) {
		std::cout << "Generating and saving " << nImages << " images (" << width << " x " << height << ") to:\n";
		std::cout << outfolder << '\n';
		std::cout << "Running " << nthreads << " threads, press 'q' to exit:\n";
		return cr::steady_clock::now();
	};

	auto showFinish = [](int nImages, bool bBreak, const cr::steady_clock::time_point tstart) {
		if (bBreak)
			std::cout << "Aborted by user.\n";
		else {
			const auto dur = cr::duration_cast<cr::milliseconds>(cr::steady_clock::now() - tstart);
			const auto sdur = std::format("{:%H:%M:%S}", dur);
			std::cout << "Finished saving " << nImages << " images, time: " << sdur << '\n';
		}
	};

	if (ntotal < 1) { // no coefficient animation file specified
		if (nsteps < 2) { // save one image
			fs::path fpath(outfolder);
			fpath.append("image.png");
			pixColorMap pcmap; pixBuf pix;
			std::cout << "Generating image...\n";
			CFracDraw frac;
			frac.DrawLozi(pcmap, pix, width, height, coef1, coef2);
			std::vector<uint8_t> buf;
			write_png_file(fpath.string().c_str(), width, height, (uint8_t*)pix.data(), buf);
			std::cout << "Saved " << width << " x " << height << " image: " << fpath.string() << '\n';
		}
		else { // save one image series
			if (nthreads > nsteps)
				nthreads = nsteps;
			const auto tstart = showStart(nsteps);
			const auto bBreak = saveSteps(0, nsteps, nthreads, coef1, coef1end, coef2, coef2end, nsteps);
			showFinish(nsteps, bBreak, tstart);
		}
	}
	else { // coefficient animation file specified
		const auto tstart = showStart(ntotal);
		const auto& lst = coef.lst();
		bool bBreak = false; int nstart = 0;
		for (auto const& line : lst) {
			nsteps = std::get<0>(line);
			bBreak = saveSteps(nstart, ntotal, std::min(nsteps, nthreads),
				std::get<1>(line), std::get<2>(line), std::get<3>(line), std::get<4>(line), nsteps);
			if (bBreak)
				break;
			nstart += nsteps;
		}
		showFinish(ntotal, bBreak, tstart);
	}

	return 0;
}
