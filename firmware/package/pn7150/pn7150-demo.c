// SPDX-License-Identifier: MIT
/* Linux front end for NXP's PN7150 NCI/NDEF Cortex-M example. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Nfc.h>

#define NDEF_MAX_SIZE 500U

static unsigned char ndef_message[NDEF_MAX_SIZE];
static unsigned int ndef_expected;
static unsigned int ndef_received;
static bool ndef_complete;

static unsigned char discovery_technologies[] = {
	MODE_POLL | TECH_PASSIVE_NFCA,
	MODE_POLL | TECH_PASSIVE_NFCF,
	MODE_POLL | TECH_PASSIVE_NFCB,
	MODE_POLL | TECH_PASSIVE_15693,
};

static void print_hex(const char *label, const unsigned char *data,
		      unsigned int length)
{
	unsigned int i;

	printf("%s", label);
	for (i = 0; i < length; i++)
		printf(" %02X", data[i]);
	putchar('\n');
}

static void print_text(const unsigned char *data, unsigned int length)
{
	unsigned int i;

	for (i = 0; i < length; i++) {
		unsigned char c = data[i];

		putchar((c >= 0x20 && c < 0x7f) || c == '\t' ? c : '.');
	}
}

static const char *uri_prefix(unsigned char code)
{
	static const char *const prefixes[] = {
		"", "http://www.", "https://www.", "http://", "https://",
		"tel:", "mailto:", "ftp://anonymous:anonymous@", "ftp://ftp.",
		"ftps://", "sftp://", "smb://", "nfs://", "ftp://", "dav://",
		"news:", "telnet://", "imap:", "rtsp://", "urn:", "pop:",
		"sip:", "sips:", "tftp:", "btspp://", "btl2cap://",
		"btgoep://", "tcpobex://", "irdaobex://", "file://",
		"urn:epc:id:", "urn:epc:tag:", "urn:epc:pat:", "urn:epc:raw:",
		"urn:epc:", "urn:nfc:",
	};

	if (code < sizeof(prefixes) / sizeof(prefixes[0]))
		return prefixes[code];
	return "";
}

static void parse_ndef(const unsigned char *message, unsigned int length)
{
	unsigned int offset = 0;
	unsigned int record_number = 0;

	printf("NDEF message: %u byte(s)\n", length);
	while (offset < length) {
		unsigned char flags;
		unsigned int type_length;
		unsigned int id_length = 0;
		unsigned int payload_length;
		unsigned int header_length;
		const unsigned char *type;
		const unsigned char *payload;

		if (length - offset < 3) {
			printf("Malformed NDEF record header at byte %u\n", offset);
			return;
		}
		flags = message[offset];
		type_length = message[offset + 1];
		if ((flags & 0x10U) != 0) {
			payload_length = message[offset + 2];
			header_length = 3;
		} else {
			if (length - offset < 6) {
				printf("Malformed long NDEF record at byte %u\n", offset);
				return;
			}
			payload_length = ((unsigned int)message[offset + 2] << 24) |
				((unsigned int)message[offset + 3] << 16) |
				((unsigned int)message[offset + 4] << 8) |
				message[offset + 5];
			header_length = 6;
		}
		if ((flags & 0x08U) != 0) {
			if (offset + header_length >= length) {
				printf("Malformed NDEF ID field at byte %u\n", offset);
				return;
			}
			id_length = message[offset + header_length++];
		}
		if (type_length > length - offset - header_length ||
		    id_length > length - offset - header_length - type_length ||
		    payload_length > length - offset - header_length - type_length - id_length) {
			printf("NDEF record at byte %u exceeds the message\n", offset);
			return;
		}
		type = message + offset + header_length;
		payload = type + type_length + id_length;
		record_number++;
		printf("Record %u: TNF=%u, type=", record_number, flags & 0x07U);
		if (type_length != 0)
			print_text(type, type_length);
		else
			printf("(empty)");
		printf(", payload=%u byte(s)\n", payload_length);

		if ((flags & 0x07U) == 1 && type_length == 1 && type[0] == 'T' &&
		    payload_length >= 1) {
			unsigned int language_length = payload[0] & 0x3fU;

			if (language_length + 1 <= payload_length) {
				printf("  Text: ");
				print_text(payload + 1 + language_length,
					   payload_length - 1 - language_length);
				putchar('\n');
			}
		} else if ((flags & 0x07U) == 1 && type_length == 1 && type[0] == 'U' &&
			   payload_length >= 1) {
			printf("  URI: %s", uri_prefix(payload[0]));
			print_text(payload + 1, payload_length - 1);
			putchar('\n');
		} else {
			print_hex("  Payload:", payload, payload_length);
		}

		offset += header_length + type_length + id_length + payload_length;
		if ((flags & 0x40U) != 0)
			break;
	}
}

static void ndef_pull_callback(unsigned char *fragment, unsigned short fragment_size,
			       unsigned int message_size)
{
	if (fragment == NULL || message_size == 0) {
		printf("Tag has no readable NDEF message (or it exceeds the library buffer)\n");
		return;
	}
	if (message_size > sizeof(ndef_message) ||
	    fragment_size > sizeof(ndef_message) - ndef_received) {
		printf("NDEF message is too large: %u byte(s), maximum is %u\n",
		       message_size, (unsigned int)sizeof(ndef_message));
		return;
	}
	if (ndef_received == 0)
		ndef_expected = message_size;
	memcpy(ndef_message + ndef_received, fragment, fragment_size);
	ndef_received += fragment_size;
	printf("NDEF data: received %u/%u byte(s)\n", ndef_received, ndef_expected);
	if (ndef_received >= ndef_expected) {
		ndef_complete = true;
		parse_ndef(ndef_message, ndef_expected);
	}
}

static const char *protocol_name(unsigned char protocol)
{
	switch (protocol) {
	case PROT_T1T: return "NFC Forum Type 1";
	case PROT_T2T: return "NFC Forum Type 2";
	case PROT_T3T: return "NFC Forum Type 3";
	case PROT_ISODEP: return "NFC Forum Type 4 / ISO-DEP";
	case PROT_T5T: return "NFC Forum Type 5 / ISO 15693";
	case PROT_MIFARE: return "MIFARE Classic";
	default: return "unknown";
	}
}

static void print_tag(const NxpNci_RfIntf_t *tag)
{
	printf("Tag discovered: %s (protocol 0x%02X, interface 0x%02X)\n",
	       protocol_name(tag->Protocol), tag->Protocol, tag->Interface);
	if (tag->ModeTech == (MODE_POLL | TECH_PASSIVE_NFCA)) {
		print_hex("  NFCID:", tag->Info.NFC_APP.NfcId,
			  tag->Info.NFC_APP.NfcIdLen);
		print_hex("  SENS_RES:", tag->Info.NFC_APP.SensRes, 2);
		if (tag->Info.NFC_APP.SelResLen != 0)
			print_hex("  SEL_RES:", tag->Info.NFC_APP.SelRes, 1);
	} else if (tag->ModeTech == (MODE_POLL | TECH_PASSIVE_15693)) {
		print_hex("  ID:", tag->Info.NFC_VPP.ID, sizeof(tag->Info.NFC_VPP.ID));
	}
}

static int connect_and_configure(void)
{
	unsigned char firmware[3];

	printf("Connecting with NXP-NCI Cortex-M library 1.6...\n");
	if (NxpNci_Connect() != NFC_SUCCESS) {
		fprintf(stderr, "Cannot connect to the PN7150\n");
		return -1;
	}
	NxpNci_GetFwVersion(firmware);
	printf("PN7150 firmware: %02X.%02X.%02X\n", firmware[0], firmware[1], firmware[2]);
	if (NxpNci_ConfigureSettings() != NFC_SUCCESS) {
		fprintf(stderr, "Cannot apply PN7150 board/RF settings\n");
		return -1;
	}
	if (NxpNci_ConfigureMode(NXPNCI_MODE_RW) != NFC_SUCCESS) {
		fprintf(stderr, "Cannot configure PN7150 reader/writer mode\n");
		return -1;
	}
	return 0;
}

static int command_info(void)
{
	int result = connect_and_configure();

	NxpNci_Disconnect();
	if (result == 0)
		printf("PN7150 initialization successful\n");
	return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int command_poll(void)
{
	NxpNci_RfIntf_t tag;
	int result = EXIT_FAILURE;

	RW_NDEF_RegisterPullCallback(ndef_pull_callback);
	if (connect_and_configure() < 0)
		goto out;
	if (NxpNci_StartDiscovery(discovery_technologies,
				  sizeof(discovery_technologies)) != NFC_SUCCESS) {
		fprintf(stderr, "Cannot start NFC discovery\n");
		goto out;
	}
	printf("Waiting for one NFC tag...\n");
	if (NxpNci_WaitForDiscoveryNotification(&tag) != NFC_SUCCESS) {
		fprintf(stderr, "NFC discovery failed\n");
		goto stop;
	}
	print_tag(&tag);
	ndef_expected = 0;
	ndef_received = 0;
	ndef_complete = false;
	NxpNci_ProcessReaderMode(tag, READ_NDEF);
	if (!ndef_complete)
		printf("No complete NDEF message was returned\n");
	result = EXIT_SUCCESS;
stop:
	NxpNci_StopDiscovery();
out:
	NxpNci_Disconnect();
	return result;
}

static void usage(const char *program)
{
	printf("Usage: %s info|poll\n", program);
	printf("  info  initialize the PN7150 and print its firmware version\n");
	printf("  poll  discover one tag and read/parse its NDEF message\n");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	if (argc != 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (strcmp(argv[1], "info") == 0)
		return command_info();
	if (strcmp(argv[1], "poll") == 0)
		return command_poll();
	usage(argv[0]);
	return EXIT_FAILURE;
}
