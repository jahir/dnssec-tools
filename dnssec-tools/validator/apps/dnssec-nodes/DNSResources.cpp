#include "DNSResources.h"

DNSResources::DNSResources()
{
}

const char *DNSResources::typeToRRName(int type)
{
    switch (type) {
    case 1:     return "A";
    case 2:     return "NS";
    case 5:     return "CNAME";
    case 6:     return "SOA";
    case 12:    return "PTR";
    case 15:    return "MX";
    case 16:    return "TXT";
    case 28:    return "AAAA";
    case 33:    return "SRV";
    case 255:   return "ANY";

    case 43:    return "DS";
    case 46:    return "RRSIG";
    case 47:    return "NSEC";
    case 48:    return "DNSKEY";
    case 50:    return "NSEC3";
    case 32769: return "DLV";

    case 3:     return "MD";
    case 4:     return "MF";
    case 7:     return "MB";
    case 8:     return "MG";
    case 9:     return "MR";
    case 10:    return "NULL";
    case 11:    return "WKS";
    case 13:    return "HINFO";
    case 14:    return "MINFO";
    case 17:    return "RP";
    case 18:    return "AFSDB";
    case 19:    return "X25";
    case 20:    return "ISDN";
    case 21:    return "RT";
    case 22:    return "NSAP";
    case 23:    return "NSAP_PTR";
    case 24:    return "SIG";
    case 25:    return "KEY";
    case 26:    return "PX";
    case 27:    return "GPOS";
    case 29:    return "LOC";
    case 30:    return "NXT";
    case 31:    return "EID";
    case 32:    return "NIMLOC";
    case 34:    return "ATMA";
    case 35:    return "NAPTR";
    case 36:    return "KX";
    case 37:    return "CERT";
    case 38:    return "A6";
    case 39:    return "DNAME";
    case 40:    return "SINK";
    case 41:    return "OPT";
    case 250:   return "TSIG";
    case 251:   return "IXFR";
    case 252:   return "AXFR";
    case 253:   return "MAILB";
    case 254:   return "MAILA";
    }

    return "(type unknown)";
}

int DNSResources::RRNameToType(const QString &name)
{
    if (name == "A")
        return 1;
    if (name == "NS")
        return 2;
    if (name == "CNAME")
        return 5;
    if (name == "SOA")
        return 6;
    if (name == "PTR")
        return 12;
    if (name == "MX")
        return 15;
    if (name == "TXT")
        return 16;
    if (name == "AAAA")
        return 28;
    if (name == "SRV")
        return 33;
    if (name == "ANY")
        return 255;

    if (name == "DS")
        return 43;
    if (name == "RRSIG")
        return 46;
    if (name == "NSEC")
        return 47;
    if (name == "DNSKEY")
        return 48;
    if (name == "NSEC3")
        return 50;
    if (name == "DLV")
        return 32769;

    if (name == "MD")
        return 3;
    if (name == "MF")
        return 4;
    if (name == "MB")
        return 7;
    if (name == "MG")
        return 8;
    if (name == "MR")
        return 9;
    if (name == "NULL")
        return 10;
    if (name == "WKS")
        return 11;
    if (name == "HINFO")
        return 13;
    if (name == "MINFO")
        return 14;
    if (name == "RP")
        return 17;
    if (name == "AFSDB")
        return 18;
    if (name == "X25")
        return 19;
    if (name == "ISDN")
        return 20;
    if (name == "RT")
        return 21;
    if (name == "NSAP")
        return 22;
    if (name == "NSAP_PTR")
        return 23;
    if (name == "SIG")
        return 24;
    if (name == "KEY")
        return 25;
    if (name == "PX")
        return 26;
    if (name == "GPOS")
        return 27;
    if (name == "LOC")
        return 29;
    if (name == "NXT")
        return 30;
    if (name == "EID")
        return 31;
    if (name == "NIMLOC")
        return 32;
    if (name == "ATMA")
        return 34;
    if (name == "NAPTR")
        return 35;
    if (name == "KX")
        return 36;
    if (name == "CERT")
        return 37;
    if (name == "A6")
        return 38;
    if (name == "DNAME")
        return 39;
    if (name == "SINK")
        return 40;
    if (name == "OPT")
        return 41;
    if (name == "TSIG")
        return 250;
    if (name == "IXFR")
        return 251;
    if (name == "AXFR")
        return 252;
    if (name == "MAILB")
        return 253;
    if (name == "MAILA")
        return 254;
    return -1;
}

QStringList DNSResources::dnsDataToQStringList(const char *buf, size_t buf_len) {
    ns_msg          handle;
    int             rrnum = 0;
    ns_rr           rr;
    QStringList     results;

    if (ns_initparse((u_char *) buf, buf_len, &handle) < 0) {
        return results;
    }

    for (;;) {
        if (ns_parserr(&handle, ns_s_an, rrnum, &rr)) {
            if (errno != ENODEV) {
                /* parse error */
                continue;
            }
            break; /* out of data */
        }

        QString data = DNSResources::rrDataToQString(rr, ns_msg_base(handle), ns_msg_size(handle));
        if (data.length() > 0)
            results.push_back(data);

        rrnum++;
    }
    return results;
}
extern "C" {

int	ns_sprintrrf_data(const u_char *, size_t, const char *,
        ns_class, ns_type, u_long, const u_char *,
        size_t, const char *,
        char *, size_t);
}
QString DNSResources::rrDataToQString(ns_rr rr, const u_char *msgBase, size_t msgSize)
{
    char buf[4096];
    const char *addr;
    Q_UNUSED(addr);
    size_t buflen = sizeof(buf);
    int len;

    len = ns_sprintrrf_data(msgBase, msgSize,
                            ns_rr_name(rr), ns_rr_class(rr), ns_rr_type(rr),
                            ns_rr_ttl(rr), ns_rr_rdata(rr), ns_rr_rdlen(rr),
                            "dnssec-tools.org" /* XXX: origin */, buf, buflen);
    if (len < 0)
        return(QString());

    return QString(buf);
}
