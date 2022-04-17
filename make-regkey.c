/*
 * Registration key maker for Stereo Tool
 * Copyright (C) 2021 Anthony96922
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>

//#define DUMP_BITS

// max name length
#define MAXLEN		108

// uncomment for event FM license (3 days)
//#define EVENT_FM

// known features
#define FEATURE_FM_PROC		(0x00000001 | 0x00000004 | 0x00000008)
#define FEATURE_ADV_CLIPPER	0x00000002 // also enables dehummer
#define FEATURE_ADVANCED_RDS	0x00000010
#define FEATURE_DEHUMMER_ONLY	0x00000020
#define FEATURE_FILE_POLLING	0x00000040
#define FEATURE_LOW_LAT_MON	0x00000080
#define FEATURE_DECLIPPER	0x00000800
#define FEATURE_DECLIPPER_2H	0x00001000
#define FEATURE_NAT_DYNAMICS	0x00004000 // not set when declipper is enabled
#define FEATURE_EVENT_FM_PROC	0x00008000
#define FEATURE_COMP_CLIP	0x00010000
#define FEATURE_EVENT_COMP_CLIP	(0x00010000 | 0x00020000)
#define FEATURE_DELOSSIFIER	0x00040000
#define FEATURE_UMPX_DISABLE	0x00080000
#define FEATURE_AGC34_AEQ	0x00200000
#define FEATURE_DYN_SPEEDS	0x00400000
#define FEATURE_BIMP		0x00800000
#define FEATURE_UMPXP_DISABLE	0x10000000

#ifdef EVENT_FM
#define FEATURE_FM	FEATURE_EVENT_FM_PROC | \
			FEATURE_ADVANCED_RDS | \
			FEATURE_EVENT_COMP_CLIP
#else
#define FEATURE_FM	FEATURE_FM_PROC | \
			FEATURE_ADVANCED_RDS | \
			FEATURE_COMP_CLIP
#endif

// feature mask
#define FEATURES	FEATURE_ADV_CLIPPER | FEATURE_FILE_POLLING | \
			FEATURE_LOW_LAT_MON | FEATURE_FM | \
			FEATURE_DECLIPPER | FEATURE_DELOSSIFIER | \
			FEATURE_AGC34_AEQ | FEATURE_DYN_SPEEDS | \
			FEATURE_BIMP

#ifdef DUMP_BITS
static void dump_bit32(unsigned int value) {
	for (int i = 0; i < 8; i++) {
		printf(" %d | %d %d %d %d\n", 7 - i,
			value >> (31 - (i * 4 + 0)) & 1,
			value >> (31 - (i * 4 + 1)) & 1,
			value >> (31 - (i * 4 + 2)) & 1,
			value >> (31 - (i * 4 + 3)) & 1);
	}
}
#endif

int main(int argc, char *argv[]) {
	int opt;
	unsigned int features = FEATURES;
	int name_len;
	int key_len;
	// key checksum
	int checksum;
	char name[MAXLEN+1];
	unsigned char key[9+MAXLEN+1+8];
	char out_key_text[(9+MAXLEN+1+8)*2];

	unsigned char *key_features;
	unsigned char *key_checksum;
	unsigned char *key_name;
	unsigned char *key_trailer;

	const char *short_opt = "f:h";
	const struct option long_opt[] = {
		{"features",	required_argument,	NULL,	'f'},
		{"help",	no_argument,		NULL,	'h'},
		{0,		0,			0,	0}
	};

	memset(name, 0, MAXLEN+1);
	memset(key, 0, 9+MAXLEN+1+8);
	memset(out_key_text, 0, (9+MAXLEN+1+8)*2);

keep_parsing_opts:

	opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
	if (opt == -1) goto done_parsing_opts;

	switch (opt) {
		case 'f':
			features = strtoul(optarg, NULL, 16);
			break;

		case 'h':
		case '?':
		default:
			fprintf(stderr,
				"Stereo Tool key generator\n"
				"\n"
				"Generates a valid registration key for a given name\n"
				"\n"
				"Usage: %s [ -f features (hex) ] NAME\n"
				"\n"
				"\t-f features\tRegistered options in hexadecimal (optional)\n"
				"\n",
			argv[0]);
			return 1;
	}

	goto keep_parsing_opts;

done_parsing_opts:

	if (optind < argc) {
		name_len = strlen(argv[optind]);
		if (name_len > MAXLEN) {
			printf("Name is too long.\n");
			return 1;
		}
		strncat(name, argv[optind], MAXLEN);
	}

	if (!name[0]) {
		printf("Please specify a name.\n");
		return 1;
	}

	// input validation

	// needed for name validation
	if (name_len < 5) {
		printf("Name must be at least 5 characters long.\n");
		return 1;
	}

	// make sure we don't try to divide by 0
	if ((name[2] - name[3]) + 1 == 0) {
		// we can't divide by 0
		printf("Invalid name.\n");
		return 1;
	}

#ifdef DUMP_BITS
	dump_bit32(features);
#endif

	/*
	 * 18 = the stuff before and after the key (112233445566778899<name>00aabbccddeeffaabb)
	 * 14 (9 + name_len + 1 + 4) is the bare minimum
	 */
	key_len = 9 + name_len + 1 /* null terminator for name string */ + 8;

	// the locations of the important parts
	key_features	= key + 1; // licensed features
	key_checksum	= key + 5;
	key_name	= key + 9; // display name
	key_trailer	= key_name + name_len + 1; // name validation

	key[0] = 1; // doesn't seem to affect anything

	// registered options
	memcpy(key_features, &features, sizeof(int));

	// copy name to key
	memcpy(key_name, name, name_len);

	// the algorithm as found on ghidra
	key_trailer[0] = (((name[0] | name[1]) ^ ((name[2] | name[3]) + name[4])) & 0xf) << 4;
	key_trailer[0] |= (name[0] ^ name[1] ^ name[2] ^ name[3] ^ name[4]) & 0xf;
	key_trailer[1] = (((name[0] * name[1]) / ((name[2] - name[3]) + 1) - name[4]) & 0xf) << 4;
	key_trailer[1] |= ((name[0] * name[1]) / ((name[2] - name[3]) + 1) * name[4]) & 0xf;
	key_trailer[2] = (((name[2] + name[3]) * (name[0] - name[1]) ^ ~name[4]) & 0xf) << 4;
	key_trailer[2] |= ((name[2] - name[3]) * (name[0] + name[1]) ^ name[4]) & 0xf;
	key_trailer[3] = ((((name[0] ^ name[1]) + (name[2] ^ name[3])) ^ name[4]) & 0xf) << 4;
	key_trailer[3] |= (name[0] + name[1] + name[2] - name[3] - name[4]) & 0xf;
#if 0 // these are not determined yet
	key_trailer[4] = 0;
	key_trailer[5] = 0;
	key_trailer[6] = 0;
	key_trailer[7] = 0;
#endif

	// calculate the checksum
	checksum = 0;
	for (int i = 0; i < key_len; i++) {
		checksum = key[i] * 0x11121 + (checksum << 3);
		checksum += checksum >> 26;
	}

	// copy the checksum
	memcpy(key_checksum, &checksum, sizeof(int));

	// encode the key
	char in, out;
	for (int i = 0; i < key_len; i++) {
		in = key[i] ^ ((-1 - i) - (1 << (1 << (i & 31) & 7)));
		out = 0;
		for (int j = 0; j < 8; j++) {
			out <<= 1;
			out |= in & 1;
			in >>= 1;
		}
		sprintf(out_key_text+i*2, "%02x", out);
	}

	// output
	printf("\n");
	printf("==========================================\n");
	printf("Name\t\t: %s\n", name);
	printf("Features\t: 0x%08x\n", features);
	printf("==========================================\n");
	printf("\n");
	printf("<%s>\n", out_key_text);
	printf("\n");

	return 0;
}
