
#include <unordered_map>

/*class ScanResult {
	public:
	 virtual ToString() = 0;
	 virtual ~ScanResult();
};

class KuduScanResult : public ScanResult {
public:
	enum ScanResultType {
		TSERVER_ADDR;
		TSERVER_UUID;
		TABLET_UUID;
		TABLET_ROLE;
		TABLET_STATE;
	};
	virtual ~kuduScanResult();
	KuduScanResult(std::string uuid, std::string rpc_addr);

	Status SetTabletMeta(std::unordered_map<std::string, std::string> tabletMeta);
private:
	KuduScanResult& operator=(const KuduScanResult& other) = delete;
	KuduScanResult(const KuduScanResult&) = delete;

	std::unordered_map<std::string, std::string> meta_details_;
}*/




class Scanner {
public:
	virtual Status Scan() = 0;
}



