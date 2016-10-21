class ServiceIf {
  public:
  	virtual add() = 0;
};


class KuduServiceIf{
  public:
	virtual add() override;
	~KuduServiceIf();
	static Status CreateKuduServiceIf(std::string kudu_master_addr);
  private:
  	KuduServiceIf();

  	KuduServiceIf& operator=(const KuduServiceIf& other) = delete;
  	KuduServiceIf(const KuduServiceIf& other) = delete;

};