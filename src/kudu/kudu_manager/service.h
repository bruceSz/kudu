<<<<<<< HEAD

namespace kudu {
namespace kudu_manager {
class ServiceIf {
  public:
    virtual Status add() = 0;
=======
class ServiceIf {
  public:
  	virtual add() = 0;
>>>>>>> 73fa74c06224d2cf0d6c649a5d403958a121ee87
};


class KuduServiceIf{
  public:
<<<<<<< HEAD
  virtual add() override;
  ~KuduServiceIf();
  static Status CreateKuduServiceIf(std::string kudu_master_addr, std::shared_ptr<KuduServiceIf>* ksi);
  private:
    KuduServiceIf();

    KuduServiceIf& operator=(const KuduServiceIf& other) = delete;
    KuduServiceIf(const KuduServiceIf& other) = delete;

};
} // kudu_manager
} // kudu
=======
	virtual add() override;
	~KuduServiceIf();
	static Status CreateKuduServiceIf(std::string kudu_master_addr);
  private:
  	KuduServiceIf();

  	KuduServiceIf& operator=(const KuduServiceIf& other) = delete;
  	KuduServiceIf(const KuduServiceIf& other) = delete;

};
>>>>>>> 73fa74c06224d2cf0d6c649a5d403958a121ee87
