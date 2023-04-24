#ifndef __NETIFS_H__
#define __NETIFS_H__

#include "netif.h"

class NetInterfaces {
	private:
		std::map<std::wstring, NetInterface *> ifs;

		NetInterface * Add(const char * name);

		// copy and assignment not allowed
		NetInterfaces(const NetInterfaces&) = delete;
		void operator=(const NetInterfaces&) = delete;

	public:
		typedef std::map<std::wstring, NetInterface *>::const_iterator const_iterator;
		const_iterator begin() const { return ifs.begin(); }
		const_iterator end() const { return ifs.end(); }
		int size(void) const { return ifs.size(); };

		NetInterfaces();
		~NetInterfaces();

		int Update(void);
		void Log(void);
		void Clear(void);
};

#endif /* __NETIFS_HS__ */
