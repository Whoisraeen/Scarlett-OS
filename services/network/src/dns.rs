//! DNS (Domain Name System) Resolver

use crate::udp;
use core::mem;

/// DNS header structure
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct DnsHeader {
    pub id: u16,              // Transaction ID
    pub flags: u16,           // Flags
    pub questions: u16,       // Number of questions
    pub answers: u16,         // Number of answers
    pub authority: u16,       // Number of authority RRs
    pub additional: u16,      // Number of additional RRs
}

/// DNS question structure
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct DnsQuestion {
    // Name is variable length, encoded
    pub qtype: u16,           // Query type
    pub qclass: u16,          // Query class
}

/// DNS resource record structure
#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct DnsResourceRecord {
    // Name is variable length
    pub rtype: u16,           // RR type
    pub rclass: u16,          // RR class
    pub ttl: u32,             // Time to live
    pub rdlength: u16,        // Resource data length
    // RDATA follows
}

/// DNS query types
pub const DNS_TYPE_A: u16 = 1;       // IPv4 address
pub const DNS_TYPE_NS: u16 = 2;      // Name server
pub const DNS_TYPE_CNAME: u16 = 5;   // Canonical name
pub const DNS_TYPE_SOA: u16 = 6;     // Start of authority
pub const DNS_TYPE_PTR: u16 = 12;    // Pointer
pub const DNS_TYPE_MX: u16 = 15;     // Mail exchange
pub const DNS_TYPE_AAAA: u16 = 28;   // IPv6 address

/// DNS query class
pub const DNS_CLASS_IN: u16 = 1;     // Internet

/// DNS flags
pub const DNS_FLAG_QR: u16 = 0x8000;         // Query/Response
pub const DNS_FLAG_OPCODE: u16 = 0x7800;     // Opcode
pub const DNS_FLAG_AA: u16 = 0x0400;         // Authoritative answer
pub const DNS_FLAG_TC: u16 = 0x0200;         // Truncated
pub const DNS_FLAG_RD: u16 = 0x0100;         // Recursion desired
pub const DNS_FLAG_RA: u16 = 0x0080;         // Recursion available
pub const DNS_FLAG_RCODE: u16 = 0x000F;      // Response code

/// DNS cache entry
#[derive(Clone, Copy)]
pub struct DnsCacheEntry {
    pub name: [u8; 256],
    pub name_len: usize,
    pub ip: u32,
    pub ttl: u32,
    pub timestamp: u64,
    pub valid: bool,
}

const DNS_CACHE_SIZE: usize = 128;
static mut DNS_CACHE: [DnsCacheEntry; DNS_CACHE_SIZE] = [DnsCacheEntry {
    name: [0; 256],
    name_len: 0,
    ip: 0,
    ttl: 0,
    timestamp: 0,
    valid: false,
}; DNS_CACHE_SIZE];

static mut DNS_SERVER: u32 = 0;
static mut NEXT_DNS_ID: u16 = 1;

/// Initialize DNS resolver
pub fn dns_init(dns_server: u32) -> Result<(), ()> {
    unsafe {
        DNS_SERVER = dns_server;
        NEXT_DNS_ID = 1;

        // Clear cache
        for i in 0..DNS_CACHE_SIZE {
            DNS_CACHE[i].valid = false;
        }
    }

    Ok(())
}

/// Encode domain name in DNS format
fn encode_domain_name(domain: &str, buffer: &mut [u8]) -> usize {
    let mut offset = 0;

    for label in domain.split('.') {
        if label.len() > 63 {
            return 0; // Label too long
        }

        buffer[offset] = label.len() as u8;
        offset += 1;

        buffer[offset..offset + label.len()].copy_from_slice(label.as_bytes());
        offset += label.len();
    }

    buffer[offset] = 0; // Null terminator
    offset + 1
}

/// Decode domain name from DNS format
fn decode_domain_name(packet: &[u8], mut offset: usize, buffer: &mut [u8]) -> (usize, usize) {
    let mut buf_offset = 0;
    let start_offset = offset;
    let mut jumped = false;
    let mut jump_count = 0;

    loop {
        if jump_count > 10 {
            // Prevent infinite loops from malformed packets
            return (0, start_offset);
        }

        let len = packet[offset] as usize;

        if len == 0 {
            // End of name
            if !jumped {
                offset += 1;
            }
            break;
        } else if (len & 0xC0) == 0xC0 {
            // Pointer
            if !jumped {
                offset += 2;
                jumped = true;
            }
            let ptr = (((len & 0x3F) as usize) << 8) | (packet[offset + 1] as usize);
            offset = ptr;
            jump_count += 1;
        } else {
            // Label
            if buf_offset > 0 {
                buffer[buf_offset] = b'.';
                buf_offset += 1;
            }

            let label_len = len.min(buffer.len() - buf_offset);
            buffer[buf_offset..buf_offset + label_len]
                .copy_from_slice(&packet[offset + 1..offset + 1 + label_len]);
            buf_offset += label_len;

            if !jumped {
                offset += len + 1;
            }
        }
    }

    (buf_offset, if jumped { start_offset + 2 } else { offset })
}

/// Lookup domain name in cache
fn dns_cache_lookup(domain: &str) -> Option<u32> {
    unsafe {
        for i in 0..DNS_CACHE_SIZE {
            if DNS_CACHE[i].valid {
                let cached_name = core::str::from_utf8(&DNS_CACHE[i].name[0..DNS_CACHE[i].name_len])
                    .ok()?;
                if cached_name == domain {
                    // TODO: Check TTL and timestamp
                    return Some(DNS_CACHE[i].ip);
                }
            }
        }
    }

    None
}

/// Add entry to DNS cache
fn dns_cache_add(domain: &str, ip: u32, ttl: u32) {
    unsafe {
        // Find free slot
        for i in 0..DNS_CACHE_SIZE {
            if !DNS_CACHE[i].valid {
                let len = domain.len().min(255);
                DNS_CACHE[i].name[0..len].copy_from_slice(&domain.as_bytes()[0..len]);
                DNS_CACHE[i].name_len = len;
                DNS_CACHE[i].ip = ip;
                DNS_CACHE[i].ttl = ttl;
                DNS_CACHE[i].timestamp = 0; // TODO: Get current time
                DNS_CACHE[i].valid = true;
                return;
            }
        }

        // Cache full, evict oldest entry
        DNS_CACHE[0].name[0..domain.len()].copy_from_slice(domain.as_bytes());
        DNS_CACHE[0].name_len = domain.len();
        DNS_CACHE[0].ip = ip;
        DNS_CACHE[0].ttl = ttl;
        DNS_CACHE[0].timestamp = 0;
        DNS_CACHE[0].valid = true;
    }
}

/// Resolve domain name to IP address
pub fn dns_resolve(domain: &str) -> Result<u32, ()> {
    // Check cache first
    if let Some(ip) = dns_cache_lookup(domain) {
        return Ok(ip);
    }

    unsafe {
        if DNS_SERVER == 0 {
            return Err(()); // No DNS server configured
        }

        // Build DNS query
        let mut packet = [0u8; 512];
        let mut offset = 0;

        // DNS header
        let id = NEXT_DNS_ID;
        NEXT_DNS_ID = NEXT_DNS_ID.wrapping_add(1);

        let header = DnsHeader {
            id: id.to_be(),
            flags: DNS_FLAG_RD.to_be(), // Recursion desired
            questions: 1u16.to_be(),
            answers: 0,
            authority: 0,
            additional: 0,
        };

        let header_bytes = core::slice::from_raw_parts(
            &header as *const _ as *const u8,
            mem::size_of::<DnsHeader>()
        );
        packet[offset..offset + mem::size_of::<DnsHeader>()].copy_from_slice(header_bytes);
        offset += mem::size_of::<DnsHeader>();

        // Encode domain name
        let name_len = encode_domain_name(domain, &mut packet[offset..]);
        if name_len == 0 {
            return Err(());
        }
        offset += name_len;

        // Question type and class
        packet[offset..offset + 2].copy_from_slice(&DNS_TYPE_A.to_be_bytes());
        offset += 2;
        packet[offset..offset + 2].copy_from_slice(&DNS_CLASS_IN.to_be_bytes());
        offset += 2;

        // Send DNS query via UDP
        udp::udp_send(DNS_SERVER, 53, 12345, &packet[0..offset])?;

        // Wait for response
        let mut response = [0u8; 512];
        for _ in 0..100 {
            if let Ok((len, src_ip, src_port, _)) = udp::udp_receive(&mut response) {
                if src_ip == DNS_SERVER && src_port == 53 && len >= mem::size_of::<DnsHeader>() {
                    // Parse response
                    let resp_header = &*(response.as_ptr() as *const DnsHeader);

                    if u16::from_be(resp_header.id) == id {
                        // Correct response
                        let answers = u16::from_be(resp_header.answers);

                        if answers > 0 {
                            // Skip questions
                            let mut resp_offset = mem::size_of::<DnsHeader>();

                            // Skip question name
                            let mut name_buf = [0u8; 256];
                            let (_, new_offset) = decode_domain_name(&response, resp_offset, &mut name_buf);
                            resp_offset = new_offset;

                            // Skip question type and class
                            resp_offset += 4;

                            // Parse answer
                            let (_, new_offset) = decode_domain_name(&response, resp_offset, &mut name_buf);
                            resp_offset = new_offset;

                            // Read RR type, class, TTL, and data length
                            let rtype = u16::from_be_bytes([response[resp_offset], response[resp_offset + 1]]);
                            resp_offset += 2;
                            let _rclass = u16::from_be_bytes([response[resp_offset], response[resp_offset + 1]]);
                            resp_offset += 2;
                            let ttl = u32::from_be_bytes([
                                response[resp_offset],
                                response[resp_offset + 1],
                                response[resp_offset + 2],
                                response[resp_offset + 3],
                            ]);
                            resp_offset += 4;
                            let rdlength = u16::from_be_bytes([response[resp_offset], response[resp_offset + 1]]);
                            resp_offset += 2;

                            if rtype == DNS_TYPE_A && rdlength == 4 {
                                // IPv4 address
                                let ip = u32::from_be_bytes([
                                    response[resp_offset],
                                    response[resp_offset + 1],
                                    response[resp_offset + 2],
                                    response[resp_offset + 3],
                                ]);

                                // Add to cache
                                dns_cache_add(domain, ip, ttl);

                                return Ok(ip);
                            }
                        }
                    }
                }
            }

            // Yield CPU
            crate::syscalls::sys_yield();
        }
    }

    Err(()) // Timeout
}

/// Reverse DNS lookup (PTR)
pub fn dns_reverse_lookup(ip: u32) -> Result<[u8; 256], ()> {
    // TODO: Implement reverse DNS lookup
    let _ = ip;
    Err(())
}
