#!/usr/bin/gawk -f
# (c) Raffael Himmelreich <raffi@raffi.at>
# This code underlies the terms of the GPL.

function fill_field(fld, hght, wdth, bmbs) {
	srand();
	for (x=0; x<wdth; x++) {
		for (y=0; y<hght; y++) {
			if (rand() > 0.9 && bmbs > 0 &&
			    fld[y, x] == 0) {
				print "Bombe auf x="x+1" y="y+1"\n";
				fld[y, x] = 1;
				bmbs--;
			}
		}
	}

	if (bmbs > 0)
		fill_field(fld, hght, wdth, bmbs);
}

function print_field(hght, wdth) {
	if (hght >= 10) print "    ";
	else            print "   ";
	for (vert=0; vert<wdth; vert++) {
		if (vert >= 10) print " ";
		else            print "  ";
		print vert+1 " ";
	}
	print "\n";

	for (vert=0; vert<hght; vert++) {
		if (hght >= 10) print "    ";
		else            print "   ";

		for (hor=0; hor<wdth; hor++) {
			print "+---"
		}
		print "+\n"

		if (vert < 9) print " ";
		print vert+1 "  "

		for (hor=0; hor<wdth; hor++) {
			if (flag_field[vert, hor] > 0) {
				print "| F ";
			} else if (open_field[vert, hor] == 0) {
				print "| # ";
			} else if (nghbs_field[vert, hor] > 0) {
				print "| " nghbs_field[vert, hor] " ";
			} else { 
				print "|   ";
			}
		}
		print "|\n"
	}

	if (hght>=10) print "    ";
	else          print "   ";
	for (hor=0; hor<wdth; hor++) {
		print "+---"
	}
	print "+\n"
	fflush();
}

function calc_neighbours(    tmp) {
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			if (y > 0 && x > 0)
				nghbs_field[y,x] += bomb_field[y-1,x-1];
			if (y > 0)
				nghbs_field[y,x] += bomb_field[y-1,x];
			if (y > 0 && x+1 < width)
				nghbs_field[y,x] += bomb_field[y-1,x+1];
			if (x > 0)
				nghbs_field[y,x] += bomb_field[y,x-1];
			if (x+1 < width)
				nghbs_field[y,x] += bomb_field[y,x+1];
			if (x > 0 && y+1 < height)
				nghbs_field[y,x] += bomb_field[y+1,x-1];
			if (y+1 < height)
				nghbs_field[y,x] += bomb_field[y+1,x];
			if (y+1 < height && x+1 < width)
				nghbs_field[y,x] += bomb_field[y+1,x+1];
		}
	}
}

function open_cell(y_pos, x_pos) {
	open_field[y_pos, x_pos] = 1;

	if (nghbs_field[y_pos, x_pos] > 0) return;

	if (x_pos+1 < width && open_field[y_pos, x_pos+1] == 0)
		open_cell(y_pos, x_pos+1);
	if (x_pos > 0 && open_field[y_pos, x_pos-1] == 0)
		open_cell(y_pos, x_pos-1);
	if (y_pos+1 < height && open_field[y_pos+1, x_pos] == 0)
		open_cell(y_pos+1, x_pos);
	if (y_pos > 0 && open_field[y_pos-1, x_pos] == 0)
		open_cell(y_pos-1, x_pos);
}

function flag_cell(y_pos, x_pos) {
	if (open_field[y_pos, x_pos] == 1) return;
	flag_field[y_pos, x_pos] = xor(flag_field[y_pos, x_pos], 1);
}

function print_help() {
	print "Syntax: <x> <y>   Open cell.\n"\
	      "        f <x> <y> Flag cell.\n"\
	      "        end       Complete the game.\n"\
	      "        exit      Exit the game.\n"\
	      "        help      Show this message again.\n\n";
}

BEGIN {
	ORS    = "";
	height = 5;
	width  = 5;
	bombs  = 10;
	
	if (height * width < bombs) {
		print "Error: More bombs than cells!\n"
		exit(1);
	}

	fill_field(bomb_field, height, width, bombs);
	print_field(height, width);
	calc_neighbours();

	print_help();
	time = strftime("%s");
	print "> ";
	fflush();
}

/^help$/ {
	print_help();
}

/^[0-9]+ [0-9]+$/ {
	if (bomb_field[$2-1, $1-1] == 1) {
		print "Boom!\n";
		exit 1;
	}
	open_cell($2-1, $1-1);
	flag_field[$2-1, $1-1] = 0;
	print_field(height, width);
}

/^f [0-9]+ [0-9]+$/ {
	flag_cell($3-1, $2-1);
	print_field(height, width);
}

/^end$/ {
	won = 1;

	for(y=0; y<height && won == 1; y++) {
		for(x=0; x<width && won == 1; x++) {
			if (bomb_field[y,x] != flag_field[y,x]) {
				won = 0;
			}
		}
	}


	if (won == 1) {
		print "Congratulation!\n";
		print strftime("%s") - time " seconds.\n";
		exit 0;
	} else {
		print "Loser.\n";
		exit 1;
	}
}
				


/^exit$/ {
	print "Bye.\n"
	exit 0;
}

{
	print "> ";
	fflush();
}
